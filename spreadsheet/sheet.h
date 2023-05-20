#pragma once

#include <functional>
#include <map>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "cell.h"
#include "common.h"

class Sheet : public SheetInterface {
public:
    ~Sheet();
    void SetCell(Position pos, std::string text) override;

    const CellInterface* GetCell(Position pos) const override;
    CellInterface* GetCell(Position pos) override;

    void ClearCell(Position pos) override;

    Size GetPrintableSize() const override;

    void PrintValues(std::ostream& output) const override;
    void PrintTexts(std::ostream& output) const override;
private:
    void CheckCircularDependency(Position pos, const std::unique_ptr<Cell>& cell) const;
    void AddDependencies(Position pos, const std::unique_ptr<Cell>& cell);
    void RemoveDependencies(Position pos, const std::unique_ptr<Cell>& cell);
    void MakeEmptyDependentCells(const std::unique_ptr<Cell>& cell);
    void InvalidateCache(Position pos) const;
    void Print(std::ostream& output, std::function<void(std::ostream&, Position)> printer) const;

    class PrintableArea {
    public:
        void AddPosition(Position pos);
        void RemovePosition(Position pos);
        Size GetSize() const;
    private:
        using AxisIndex = int;

        void RemoveProjection(std::map<AxisIndex, int>& index_count, int axis);

        std::map<AxisIndex, int> row_index_count;
        std::map<AxisIndex, int> col_index_count;
    };

    struct KeyHash {
        std::size_t operator()(const Position& pos) const {
            return static_cast<std::size_t>(pos.row) +
                   100'000 * static_cast<std::size_t>(pos.col);
        }
    };

    struct KeyEqual {
        bool operator()(const Position& l, const Position& p) const {
            return l == p;
        }
    };

    class Node {
    public:
        using DependentCells = std::unordered_set<Position, KeyHash, KeyEqual>;
        void AddDependent(Position pos);
        void RemoveDependent(Position pos);
        const DependentCells& GetDependent() const;
    private:
        DependentCells dependent_cells_;
    };

    std::unordered_map<Position, std::unique_ptr<Cell>, KeyHash, KeyEqual> sheet_;
    std::unordered_map<Position, Node, KeyHash, KeyEqual> dependency_graph_;
    PrintableArea area_;
};
