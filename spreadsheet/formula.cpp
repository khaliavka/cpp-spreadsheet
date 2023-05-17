#include "formula.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cmath>
#include <sstream>

#include "FormulaAST.h"

using namespace std::literals;

std::ostream& operator<<(std::ostream& output, FormulaError fe) {
  return output << "#DIV/0!";
}

namespace {

class Formula : public FormulaInterface {
 public:
  // Реализуйте следующие методы:
  explicit Formula(std::string expression);
  Value Evaluate(const SheetInterface& sheet) const override;
  std::string GetExpression() const override;
  std::vector<Position> GetReferencedCells() const override;

 private:
  FormulaAST ast_;
};

Formula::Formula(std::string expression) : ast_(ParseFormulaAST(expression)) {}

FormulaInterface::Value Formula::Evaluate(const SheetInterface& sheet) const {
  std::function<double(const Position*)> function(
      [&sheet](const Position* c) -> double {
        auto cell_ptr = sheet.GetCell(*c);
        if (!cell_ptr) {
          return .0;
        }
        auto value = cell_ptr->GetValue();
        if (std::holds_alternative<std::string>(value)) {
          const auto& value_str = std::get<std::string>(value);
          if (value_str.empty()) {
              return .0;
          }
          double result;
          std::istringstream value_in{value_str};
          if (value_in >> result && value_in.eof()) {
              return result;
          }
          throw FormulaError(FormulaError::Category::Value);
        }
        if (std::holds_alternative<double>(value)) {
          double result = std::get<double>(value);
          if (!std::isinf(result) && !std::isnan(result)) {
              return result;
          }
          throw FormulaError(FormulaError::Category::Div0);
        }
        throw std::get<FormulaError>(value);
      });
  try {
      double result = ast_.Execute(function);
      if (!std::isinf(result) && !std::isnan(result)) {
          return result;
      }
      return FormulaError(FormulaError::Category::Div0);
  } catch (FormulaError& ferror) {
      return ferror;
  }
}

std::string Formula::GetExpression() const {
  std::ostringstream out;
  ast_.PrintFormula(out);
  return out.str();
}

std::vector<Position> Formula::GetReferencedCells() const {
  const auto& pos_list = ast_.GetCells();
  auto it = pos_list.begin();
  std::vector<Position> result;
  if (!pos_list.empty()) {
    result.push_back(*it);
    it = std::next(it);
  }
  for (; it != pos_list.end(); it = std::next(it)) {
    if (!(*it == result.back())) {
        result.push_back(*it);
    }
  }
  return result;
}

}  // namespace

std::unique_ptr<FormulaInterface> ParseFormula(std::string expression) {
  try {
    return std::make_unique<Formula>(std::move(expression));
  } catch (std::exception&) {
    throw FormulaException("ParseFormula():Invalid formula syntax");
  }
}
