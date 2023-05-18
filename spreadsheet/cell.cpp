#include "cell.h"
#include "sheet.h"

#include <cassert>
#include <iostream>
#include <optional>
#include <string>

Cell::Cell(Sheet& sh)
    : sheet_(sh), impl_(std::make_unique<EmptyImpl>(EmptyImpl{})) {}

Cell::Cell(Sheet& sh, std::string text)
    : sheet_(sh), impl_(nullptr) {
    Set(std::move(text));
}

Cell::~Cell() {}

void Cell::Set(std::string text) {
    if (text.empty()) {
        impl_ = std::make_unique<EmptyImpl>(EmptyImpl{});
    } else if (text[0] == FORMULA_SIGN && text.size() > 1) {
        impl_ = std::move(std::make_unique<FormulaImpl>(sheet_, text.substr(1)));
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

void Cell::InvalidateCellCache() const {
    impl_->InvalidateCache();
}

bool Cell::IsEmpty() const {
    return impl_->IsEmpty();
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

void Cell::EmptyImpl::InvalidateCache() const {
//    does nothing
}

bool Cell::EmptyImpl::IsEmpty() const {
    return true;
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

void Cell::TextImpl::InvalidateCache() const {
//    does nothing
}

bool Cell::TextImpl::IsEmpty() const {
    return false;
}

// -- FormulaImpl --

Cell::FormulaImpl::FormulaImpl(Sheet& sh, std::string expr)
    : sheet_(sh), formula_(ParseFormula(std::move(expr))) {}

CellInterface::Value Cell::FormulaImpl::GetValue() const {
    if (cache_.is_valid) {
        return cache_.value;
    }
    auto result = formula_->Evaluate(sheet_);

    if (std::holds_alternative<double>(result)) {
        cache_.value = std::get<double>(result);
    } else {
        assert(std::holds_alternative<FormulaError>(result));
        cache_.value = std::get<FormulaError>(result);
    }
    cache_.is_valid = true;
    return cache_.value;
}

std::string Cell::FormulaImpl::GetText() const {
    using namespace std::literals;
    return "="s + formula_->GetExpression();
}

std::vector<Position> Cell::FormulaImpl::GetReferencedCells() const {
    return formula_->GetReferencedCells();
}

void Cell::FormulaImpl::InvalidateCache() const {
    cache_.is_valid = false;
}

bool Cell::FormulaImpl::IsEmpty() const {
    return false;
}
