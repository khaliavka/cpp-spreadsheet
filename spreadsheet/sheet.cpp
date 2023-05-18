#include "sheet.h"

#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>
#include <optional>

#include "cell.h"
#include "common.h"

using namespace std::literals;

Sheet::~Sheet() {}

void Sheet::SetCell(Position pos, std::string text) {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Invalid position in SetCell()");
    }
    auto new_cell = std::make_unique<Cell>(*this, std::move(text)); // Can throw FormulaException
    CheckCircularDependency(pos, new_cell); // Can throw CircularDependencyException
    auto& cell_in_place = sheet_[pos.row][pos.col];
    if (cell_in_place) {
        RemoveDependencies(pos, cell_in_place);
    } else {
        area_.AddPosition(pos);
    }
    cell_in_place = std::move(new_cell);
    MakeEmptyDependentCells(cell_in_place);
    AddDependencies(pos, cell_in_place);
    InvalidateCache(pos);
}

const CellInterface* Sheet::GetCell(Position pos) const {
    return const_cast<Sheet*>(this)->GetCell(pos);
}

CellInterface* Sheet::GetCell(Position pos) {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Invalid position in GetCell()");
    }
    if (sheet_.count(pos.row) != 0 && sheet_.at(pos.row).count(pos.col) != 0) {
        return sheet_.at(pos.row).at(pos.col).get();
    }
    return nullptr;
}

void Sheet::ClearCell(Position pos) {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Invalid position in ClearCell()");
    }
    if (sheet_.count(pos.row) != 0 && sheet_.at(pos.row).count(pos.col) != 0 &&
        sheet_.at(pos.row).at(pos.col)) {
        auto& cell = sheet_.at(pos.row).at(pos.col);
        if (!cell->IsEmpty()) {
            area_.RemovePosition(pos);
            RemoveDependencies(pos, cell);
        }
        cell.reset();
    }
}

Size Sheet::GetPrintableSize() const {
    return area_.GetSize();
}

void Sheet::PrintValues(std::ostream& output) const {
    Size sz = area_.GetSize();
    for (int row = 0; row < sz.rows; ++row) {
        for (int col = 0; col < sz.cols; ++col) {
            if (col != 0) {
                output << '\t';
            }
            if (sheet_.count(row) != 0 && sheet_.at(row).count(col) != 0 &&
                sheet_.at(row).at(col)) {
                const auto& val = sheet_.at(row).at(col)->GetValue();
                if (std::holds_alternative<std::string>(val)) {
                    output << std::get<std::string>(val);
                }
                if (std::holds_alternative<double>(val)) {
                    output << std::get<double>(val);
                }
                if (std::holds_alternative<FormulaError>(val)) {
                    output << std::get<FormulaError>(val);
                }
            }
        }
        output << '\n';
    }
}

void Sheet::PrintTexts(std::ostream& output) const {
    Size sz = area_.GetSize();
    for (int row = 0; row < sz.rows; ++row) {
        for (int col = 0; col < sz.cols; ++col) {
            if (col != 0) {
                output << '\t';
            }
            if (sheet_.count(row) != 0 && sheet_.at(row).count(col) != 0 &&
                sheet_.at(row).at(col)) {
                output << sheet_.at(row).at(col)->GetText();
            }
        }
        output << '\n';
    }
}

void Sheet::CheckCircularDependency(Position pos, const std::unique_ptr<Cell>& cell) const {
    if (!cell) {
        return;
    }
    for (const auto& p : cell->GetReferencedCells()) {
        if (pos == p) {
            throw CircularDependencyException("Circular dependency detected.");
        }
        if (sheet_.count(p.row) != 0 && sheet_.at(p.row).count(p.col) != 0) {
            CheckCircularDependency(pos, sheet_.at(p.row).at(p.col));
        }
    }
}

void Sheet::AddDependencies(Position pos, const std::unique_ptr<Cell>& cell) {
    assert(cell);
    for (const auto& p : cell->GetReferencedCells()) {
        dependency_graph_[p].AddDependent(pos);
    }
}

void Sheet::RemoveDependencies(Position pos, const std::unique_ptr<Cell>& cell) {
    assert(cell);
    for (const auto& p : cell->GetReferencedCells()) {
        dependency_graph_[p].RemoveDependent(pos);
    }
}

void Sheet::MakeEmptyDependentCells(const std::unique_ptr<Cell>& cell) {
    assert(cell);
    for (const auto& p : cell->GetReferencedCells()) {
        auto& cell_in_place = sheet_[p.row][p.col];
        if (!cell_in_place) {
            cell_in_place = std::make_unique<Cell>(*this);
        }
    }
}

void Sheet::InvalidateCache(Position pos) const {
    if (dependency_graph_.count(pos) != 0) {
        for (const auto& p : dependency_graph_.at(pos).GetDependent()) {
            assert(sheet_.count(p.row) != 0 &&
                   sheet_.at(p.row).count(p.col) != 0);
            InvalidateCache(p);
            sheet_.at(p.row).at(p.col)->InvalidateCellCache();
        }
    }
}

// -- RelevantArea --

void Sheet::RelevantArea::AddPosition(Position pos) {
    ++row_index_count[pos.row];
    ++col_index_count[pos.col];
}

// existence required
void Sheet::RelevantArea::RemovePosition(Position pos) {
    assert(row_index_count.count(pos.row) != 0 &&
           col_index_count.count(pos.col) != 0);

    auto& ric = row_index_count.at(pos.row);
    if (ric == 1) {
        row_index_count.erase(pos.row);
    } else {
        --ric;
    }
    auto& cic = col_index_count.at(pos.col);
    if (cic == 1) {
        col_index_count.erase(pos.col);
    } else {
        --cic;
    }
}

Size Sheet::RelevantArea::GetSize() const {
    return {row_index_count.size() == 0
                ? 0
                : std::prev(row_index_count.end())->first + 1,
            col_index_count.size() == 0
                ? 0
                : std::prev(col_index_count.end())->first + 1};
}

// -- Node --

void Sheet::Node::AddDependent(Position pos) {
    dependent_cells_.insert(pos);
}

void Sheet::Node::RemoveDependent(Position pos) {
    dependent_cells_.erase(pos);
}

const Sheet::Node::DependentCells& Sheet::Node::GetDependent() const {
    return dependent_cells_;
}

// -- aux --

std::unique_ptr<SheetInterface> CreateSheet() {
    return std::make_unique<Sheet>();
}
