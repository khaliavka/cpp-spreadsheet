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
    auto new_cell = std::make_unique<Cell>(*this, pos, std::move(text)); // Can throw FormulaException
    CheckCircularDependency(pos, new_cell); // Can throw CircularDependencyException

    auto& insertion_place = sheet_[pos.row][pos.col];
    if (!insertion_place) {
        area_.AddPosition(pos);
    }
    insertion_place = std::move(new_cell);
    for (const auto& p : insertion_place->GetReferencedCells()) {
        auto& cell = sheet_[p.row][p.col];
        if (!cell) {
            cell = std::make_unique<Cell>(*this);
        }
    }
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

void Sheet::AddDependency(Position owner, Position dependent) {
    dependency_graph_[owner].AddDependent(dependent);
}

void Sheet::RemoveDependency(Position owner, Position dependent) {
    //    assert(dependency_graph_.count(owner) != 0);
    if (dependency_graph_.count(owner) != 0) {
        dependency_graph_.at(owner).RemoveDependent(dependent);
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


std::unique_ptr<SheetInterface> CreateSheet() {
    return std::make_unique<Sheet>();
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
