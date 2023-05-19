#include "cell.h"
#include "sheet.h"

#include <cassert>
#include <iostream>
#include <optional>
#include <string>

Cell::Cell(const Sheet& sh)
    : sheet_(sh), impl_(std::make_unique<EmptyImpl>(EmptyImpl{})) {}

Cell::Cell(const Sheet& sh, std::string text)
    : sheet_(sh), impl_(nullptr) {
    Set(std::move(text));
}

Cell::~Cell() {}

void Cell::Set(std::string text) {
    if (text.empty()) {
        impl_ = std::make_unique<EmptyImpl>(EmptyImpl{});
    } else if (text[0] == FORMULA_SIGN && text.size() > 1) {
        impl_ = std::make_unique<FormulaImpl>(sheet_, text.substr(1));
    } else {
        impl_ = std::make_unique<TextImpl>(TextImpl(std::move(text)));
    }
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

Cell::FormulaImpl::FormulaImpl(const Sheet& sh, std::string expr)
    : sheet_(sh), formula_(ParseFormula(std::move(expr))) {}

CellInterface::Value Cell::FormulaImpl::GetValue() const {
    constexpr bool cache_enabled = true;
    if constexpr (cache_enabled) {

        if (cache_) {
            return *cache_;
        }
        auto result = formula_->Evaluate(sheet_);

        if (std::holds_alternative<double>(result)) {
            cache_ = std::get<double>(result);
        } else {
            assert(std::holds_alternative<FormulaError>(result));
            cache_ = std::get<FormulaError>(result);
        }
        return *cache_;

    } else {

        auto result = formula_->Evaluate(sheet_);
        if (std::holds_alternative<double>(result)) {
            return std::get<double>(result);
        }
        assert(std::holds_alternative<FormulaError>(result));
        return std::get<FormulaError>(result);

    }
}

std::string Cell::FormulaImpl::GetText() const {
    using namespace std::literals;
    return "="s + formula_->GetExpression();
}

std::vector<Position> Cell::FormulaImpl::GetReferencedCells() const {
    return formula_->GetReferencedCells();
}

void Cell::FormulaImpl::InvalidateCache() const {
    cache_.reset();
}

bool Cell::FormulaImpl::IsEmpty() const {
    return false;
}
