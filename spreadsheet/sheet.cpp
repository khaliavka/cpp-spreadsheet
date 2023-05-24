#include "sheet.h"

#include <algorithm>
#include <cassert>
#include <functional>
#include <iostream>
#include <optional>
#include <variant>

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
    auto& cell_in_place = sheet_[pos];
    if (cell_in_place && !cell_in_place->IsEmpty()) {
        RemoveDependencies(pos, cell_in_place);
        area_.RemovePosition(pos);
    }
    cell_in_place = std::move(new_cell);
    if (!cell_in_place->IsEmpty()) {
        area_.AddPosition(pos);
    }
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
    if (sheet_.count(pos) != 0) {
        return sheet_.at(pos).get();
    }
    return nullptr;
}

void Sheet::ClearCell(Position pos) {
    if (!pos.IsValid()) {
        throw InvalidPositionException("Invalid position in ClearCell()");
    }
    if (sheet_.count(pos) != 0 && sheet_.at(pos)) {
        auto& cell = sheet_.at(pos);
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
    auto printer = [this](std::ostream& output, Position pos) {
        auto val = sheet_.at(pos)->GetValue();
        std::visit([&output](auto&& v){ output << v; }, val);
    };
    Print(output, printer);
}

void Sheet::PrintTexts(std::ostream& output) const {
    auto printer = [this](std::ostream& output, Position pos) {
        output << sheet_.at(pos)->GetText();
    };
    Print(output, printer);
}

void Sheet::CheckCircularDependency(Position pos, const std::unique_ptr<Cell>& cell) const {
    if (!cell) {
        return;
    }
    for (const auto& p : cell->GetReferencedCells()) {
        if (pos == p) {
            throw CircularDependencyException("Circular dependency detected.");
        }
        if (sheet_.count(p) != 0) {
            CheckCircularDependency(pos, sheet_.at(p));
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
        auto& cell_in_place = sheet_[p];
        if (!cell_in_place) {
            cell_in_place = std::make_unique<Cell>(*this);
        }
    }
}

void Sheet::InvalidateCache(Position pos) const {
    if (dependency_graph_.count(pos) != 0) {
        for (const auto& p : dependency_graph_.at(pos).GetDependent()) {
            assert(sheet_.count(p) != 0);
            InvalidateCache(p);
            sheet_.at(p)->InvalidateCellCache();
        }
    }
}

void Sheet::Print(std::ostream& output, std::function<void(std::ostream&, Position)> printer) const {
    assert(printer);
    Size sz = area_.GetSize();
    for (int row = 0; row < sz.rows; ++row) {
        for (int col = 0; col < sz.cols; ++col) {
            if (col != 0) {
                output << '\t';
            }
            Position pos{row, col};
            if (sheet_.count(pos) != 0 && sheet_.at(pos)) {
                printer(output, pos);
            }
        }
        output << '\n';
    }
}

// -- PrintableArea --

void Sheet::PrintableArea::AddPosition(Position pos) {
    ++row_index_count[pos.row];
    ++col_index_count[pos.col];
}

void Sheet::PrintableArea::RemovePosition(Position pos) {
    assert(row_index_count.count(pos.row) != 0 &&
           col_index_count.count(pos.col) != 0);

    RemoveProjection(row_index_count, pos.row);
    RemoveProjection(col_index_count, pos.col);
}

void Sheet::PrintableArea::RemoveProjection(std::map<AxisIndex, int>& index_count, int pos_on_axis) {
    auto& ic = index_count.at(pos_on_axis);
    if (ic == 1) {
        index_count.erase(pos_on_axis);
    } else {
        --ic;
    }
}

Size Sheet::PrintableArea::GetSize() const {
    return {row_index_count.empty() ?
                0 : std::prev(row_index_count.end())->first + 1,
            col_index_count.empty() ?
                0 : std::prev(col_index_count.end())->first + 1};
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
