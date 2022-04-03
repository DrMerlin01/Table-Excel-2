#include "../inc/cell.h"

#include <cassert>
#include <iostream>
#include <string>
#include <optional>
#include <stack>

using namespace std;

// Реализуйте следующие методы
Cell::Cell(Sheet& sheet) 
	: impl_(make_unique<EmptyImpl>()) 
	, sheet_(sheet) {
}

Cell::~Cell() {
	impl_.reset();
}

void Cell::Set(string text) {
	unique_ptr<Impl> tmp_impl;

	if (text.empty()) {
		tmp_impl = make_unique<EmptyImpl>();
	} else if (text.front() == FORMULA_SIGN && text.size() > 1) {
		try {
			tmp_impl = make_unique<FormulaImpl>(move(text.substr(1)), sheet_);
		} catch (...) {
			throw FormulaException("Sytnax error"s);
		}
	} else {
		tmp_impl = make_unique<TextImpl>(move(text));
	}

	if (IsCircular(*tmp_impl)) {
		throw CircularDependencyException("Setting this formula would introduce circular dependency!");
	}

	impl_ = move(tmp_impl);

	UpdateReferences();
	InvalidCache();
	InvalidAllDependentCaches(incoming_references_);
}

void Cell::Clear() {
	impl_ = make_unique<EmptyImpl>();
}

Cell::Value Cell::GetValue() const {
	if (!cache_.has_value()) {
		cache_ = impl_->GetValue();
	}

	return cache_.value();
}

string Cell::GetText() const {
	return impl_->GetText();
}

vector<Position> Cell::GetReferencedCells() const {
	return impl_->GetReferencedCells();
}

bool Cell::IsReferenced() const {
	return !incoming_references_.empty();
}

bool Cell::IsCircular(const Impl& impl) const {
	if (impl.GetReferencedCells().empty()) {
		return false;
	}

	unordered_set<const Cell*> referenced;
	unordered_set<const Cell*> visited;
	stack<const Cell*> to_visit;

	for (const auto &pos : impl.GetReferencedCells()) {
		referenced.insert(sheet_.GetCellByIndex(pos));
	}
	to_visit.push(this);
	while (!to_visit.empty()) {
		const Cell* current = to_visit.top();
		to_visit.pop();
		visited.insert(current);
		if (referenced.find(current) != referenced.end()) {
			return true;
		}
		for (const Cell* incoming : current->incoming_references_) {
			if (visited.find(incoming) == visited.end()) {
				to_visit.push(incoming);
			}
		}
	}

	return false;
}

void Cell::UpdateReferences() {
	for (auto outgoing : outgoing_references_) {
		outgoing->incoming_references_.erase(this);
	}
	outgoing_references_.clear();

	for (const auto &pos : impl_->GetReferencedCells()) {
		Cell* outgoing = sheet_.GetCellByIndex(pos);
		if (!outgoing) {
			sheet_.SetCell(pos, "");
			outgoing = sheet_.GetCellByIndex(pos);
		}
		outgoing_references_.insert(outgoing);
		outgoing->incoming_references_.insert(this);
	}
}

void Cell::InvalidCache() {
	cache_ = nullopt;
}

void Cell::InvalidAllDependentCaches(unordered_set<Cell*>& incoming_references) {
	for (auto cell : incoming_references) {
		cell->InvalidCache();
		cell->InvalidAllDependentCaches(cell->incoming_references_);
	}
}

// --------------------EmptyImpl---------------------
Cell::EmptyImpl::EmptyImpl()
	: Impl() {
}

Cell::Value Cell::EmptyImpl::GetValue() const {
	return string();
}

string Cell::EmptyImpl::GetText() const {
	return string();
}

vector<Position> Cell::EmptyImpl::GetReferencedCells() const {
	return {};
}

// --------------------TextImpl---------------------
Cell::TextImpl::TextImpl(string content)
	: Impl()
	, content_(move(content)) {
}

Cell::Value Cell::TextImpl::GetValue() const {
	return (content_.front() != ESCAPE_SIGN) ? content_ : content_.substr(1);
}

string Cell::TextImpl::GetText() const {
	return content_;
}

vector<Position> Cell::TextImpl::GetReferencedCells() const {
	return {};
}

// --------------------FormulaImpl---------------------
Cell::FormulaImpl::FormulaImpl(string formula, const SheetInterface &sheet)
	: Impl()
	, formula_(move(ParseFormula(formula)))
	, sheet_(sheet) {
}

Cell::Value Cell::FormulaImpl::GetValue() const {
	auto result = formula_->Evaluate(sheet_);

	if(holds_alternative<double>(result)) {
		return get<double>(result);
	} else {
		return get<FormulaError>(result);
	}
}

string Cell::FormulaImpl::GetText() const {
	return FORMULA_SIGN + formula_->GetExpression();
}

vector<Position> Cell::FormulaImpl::GetReferencedCells() const {
	return formula_->GetReferencedCells();
}