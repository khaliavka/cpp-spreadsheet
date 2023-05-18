#pragma once

#include <cassert>
#include <functional>
#include <unordered_set>

#include "common.h"
#include "formula.h"

class Sheet;

class Cell : public CellInterface {
public:
    Cell(Sheet& sh);
    Cell(Sheet& sh, std::string text);
    ~Cell();

    Value GetValue() const override;
    std::string GetText() const override;
    std::vector<Position> GetReferencedCells() const override;

    void Clear();
    void InvalidateCellCache() const;
    bool IsEmpty() const;

private:
    void Set(std::string text);

    class Impl;
    class EmptyImpl;
    class TextImpl;
    class FormulaImpl;

    Sheet& sheet_;
    std::unique_ptr<Impl> impl_;
};

class Cell::Impl {
public:
    virtual ~Impl() = default;
    virtual Value GetValue() const = 0;
    virtual std::string GetText() const = 0;
    virtual std::vector<Position> GetReferencedCells() const = 0;
    virtual void InvalidateCache() const = 0;
    virtual bool IsEmpty() const = 0;
};

class Cell::EmptyImpl : public Cell::Impl {
public:
    explicit EmptyImpl() = default;
    Value GetValue() const override;
    std::string GetText() const override;
    std::vector<Position> GetReferencedCells() const override;
    void InvalidateCache() const override;
    bool IsEmpty() const override;
};

class Cell::TextImpl : public Cell::Impl {
public:
    explicit TextImpl(std::string text);
    Value GetValue() const override;
    std::string GetText() const override;
    std::vector<Position> GetReferencedCells() const override;
    void InvalidateCache() const override;
    bool IsEmpty() const override;
private:
    std::string text_;
};

class Cell::FormulaImpl : public Cell::Impl {
public:
    explicit FormulaImpl(Sheet& sh, std::string expr);
    Value GetValue() const override;
    std::string GetText() const override;
    std::vector<Position> GetReferencedCells() const override;
    void InvalidateCache() const override;
    bool IsEmpty() const override;
private:
    struct Cache {
        Value value;
        bool is_valid = false;
    };

    Sheet& sheet_;
    std::unique_ptr<FormulaInterface> formula_;
    mutable Cache cache_;
};
