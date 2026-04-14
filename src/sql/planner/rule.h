#pragma once

#include "sql/planner/memo.h"
#include <memory>
#include <vector>

namespace stingdb::planner {

class Rule {
public:
    virtual ~Rule() = default;

    // Предикат применимости (вызывается движком Cascades)
    virtual bool Match(const std::shared_ptr<GroupExpression>& expr) const = 0;

    // Метод трансформации: генерирует новые логические или физические выражения
    virtual std::vector<std::shared_ptr<GroupExpression>> Transform(
        const std::shared_ptr<GroupExpression>& expr) const = 0;

    virtual const char* GetName() const = 0;
};

/**
 * Логическое правило (Transformation Rule): 
 * Преобразует LogicalExpression -> LogicalExpression.
 * Например: InnerJoin Commutativity (A JOIN B  =>  B JOIN A) 
 * увеличивает пространство поиска планов в Memo.
 */
class LogicalJoinCommutativityRule : public Rule {
public:
    bool Match(const std::shared_ptr<GroupExpression>& expr) const override {
        return expr->GetOperatorType() == OperatorType::LOGICAL_JOIN;
    }

    std::vector<std::shared_ptr<GroupExpression>> Transform(
        const std::shared_ptr<GroupExpression>& expr) const override {
        
        std::vector<std::shared_ptr<GroupExpression>> new_exprs;
        
        const auto& children = expr->GetChildren();
        if (children.size() == 2) {
            std::vector<GroupId> new_children = {children[1], children[0]}; // Перестановка B JOIN A
            new_exprs.push_back(std::make_shared<GroupExpression>(OperatorType::LOGICAL_JOIN, std::move(new_children)));
        }
        return new_exprs;
    }

    const char* GetName() const override { return "JoinCommutativity"; }
};

/**
 * Физическое правило (Implementation Rule):
 * Преобразует LogicalExpression -> PhysicalExpression.
 * Например: LogicalJoin -> HashJoin / NestedLoopJoin
 * CBO выбирает дешевейший PhysicalExpression на базе оценки кардинальности строк.
 */
class LogicalToPhysicalHashJoinRule : public Rule {
public:
    bool Match(const std::shared_ptr<GroupExpression>& expr) const override {
        return expr->GetOperatorType() == OperatorType::LOGICAL_JOIN; // Требуется проверка наличия Equi-Join условия (ON a.id = b.id)
    }

    std::vector<std::shared_ptr<GroupExpression>> Transform(
        const std::shared_ptr<GroupExpression>& expr) const override {
        
        std::vector<std::shared_ptr<GroupExpression>> new_exprs;
        auto children = expr->GetChildren(); // Для HashJoin дети остаются теми же
        new_exprs.push_back(std::make_shared<GroupExpression>(OperatorType::PHYSICAL_HASH_JOIN, std::move(children)));
        
        return new_exprs;
    }

    const char* GetName() const override { return "ToHashJoin"; }
};

} // namespace stingdb::planner