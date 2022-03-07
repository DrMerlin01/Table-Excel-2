#pragma once

#include "common.h"
#include "formula.h"
#include "sheet.h"

#include <functional>
#include <unordered_set>

class Sheet;

class Cell : public CellInterface {
public:
	Cell(Sheet& sheet);

	~Cell();

	void Set(std::string text);

	void Clear();

	Value GetValue() const override;

	std::string GetText() const override;

	std::vector<Position> GetReferencedCells() const override;

	bool IsReferenced() const;

private:
	class Impl {
	public:
		virtual ~Impl() = default;

		virtual Value GetValue() const = 0;

		virtual std::string GetText() const = 0;

		virtual std::vector<Position> GetReferencedCells() const = 0;
	};

	class EmptyImpl : public Impl {
	public:
		EmptyImpl();

		Value GetValue() const override;

		std::string GetText() const override;

		std::vector<Position> GetReferencedCells() const override;
	};

	class TextImpl : public Impl {
	public:
		TextImpl(std::string content);

		Value GetValue() const override;

		std::string GetText() const override; 

		std::vector<Position> GetReferencedCells() const override;

	private:
		std::string content_;
	};

	class FormulaImpl : public Impl {
	public:
		FormulaImpl(std::string formula, const SheetInterface &sheet);

		Value GetValue() const override;

		std::string GetText() const override;   

		std::vector<Position> GetReferencedCells() const override;

	private:
		std::unique_ptr<FormulaInterface> formula_;
		const SheetInterface &sheet_;
	};

	std::unique_ptr<Impl> impl_;
	Sheet& sheet_;
	std::unordered_set<Cell*> incoming_references_; //от которых зависит эта ячейка
	std::unordered_set<Cell*> outgoing_references_; //обратные связи на другие ячейки из этой
	mutable std::optional<Value> cache_;

	bool IsCircular(const Impl& impl) const;

	void UpdateReferences();

	void InvalidCache();

	void InvalidAllDependentCaches(std::unordered_set<Cell*>& incoming_references);
};