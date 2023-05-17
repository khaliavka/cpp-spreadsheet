#include "cell.h"

#include <cassert>
#include <iostream>
#include <optional>
#include <string>

Cell::Cell(const SheetInterface& sheet)
    : sheet_(sheet), impl_(std::make_unique<EmptyImpl>(EmptyImpl{})) {}

Cell::~Cell() {}

void Cell::Set(std::string text) {
    if (text.empty()) {
        impl_ = std::make_unique<EmptyImpl>(EmptyImpl{});
    } else if (text[0] == FORMULA_SIGN && text.size() > 1) {
        impl_ = std::make_unique<FormulaImpl>(FormulaImpl(sheet_, text.substr(1)));
    } else {
        impl_ = std::make_unique<TextImpl>(TextImpl(std::move(text)));
    }
}

void Cell::Clear() {
    impl_ = std::make_unique<EmptyImpl>(EmptyImpl{});
}

Cell::Value Cell::GetValue() const {
    return impl_->GetValue();
}

std::string Cell::GetText() const {
    return impl_->GetText();
}

std::vector<Position> Cell::GetReferencedCells() const {
    return impl_->GetReferencedCells();
}

bool Cell::IsReferenced() const {
    return true;
}

// -- EmptyImpl --

CellInterface::Value Cell::EmptyImpl::GetValue() const {
    return std::string{};
}

std::string Cell::EmptyImpl::GetText() const {
    return std::string{};
}

std::vector<Position> Cell::EmptyImpl::GetReferencedCells() const {
    return {};
}

// -- TextImpl --

Cell::TextImpl::TextImpl(std::string text) : text_(std::move(text)) {}

CellInterface::Value Cell::TextImpl::GetValue() const {
    assert(!text_.empty());
    if (text_[0] == ESCAPE_SIGN) {
        return text_.substr(1);
    }
    return text_;
}

std::string Cell::TextImpl::GetText() const {
    return text_;
}

std::vector<Position> Cell::TextImpl::GetReferencedCells() const {
    return {};
}

// -- FormulaImpl --

Cell::FormulaImpl::FormulaImpl(const SheetInterface& sh, std::string expr)
    : sheet_(sh), formula_(ParseFormula(std::move(expr))) {}

CellInterface::Value Cell::FormulaImpl::GetValue() const {
    auto result = formula_->Evaluate(sheet_);
    if (std::holds_alternative<double>(result)) {
        return std::get<double>(result);
    }
    return std::get<FormulaError>(result);
}

std::string Cell::FormulaImpl::GetText() const {
    using namespace std::literals;
    return "="s + formula_->GetExpression();
}

std::vector<Position> Cell::FormulaImpl::GetReferencedCells() const {
    return formula_->GetReferencedCells();
}
