#pragma once

#include "sql/planner/memo.h"
#include "sql/planner/rule.h"
#include <vector>
#include <memory>

namespace stingdb::planner {

/**
 * Optimizer: Ядро Cost-Based Optimizer (CBO). Фреймворк Cascades.
 * Алгоритм (упрощенно):
 * 1. Инициализация `Memo` из корневого AST (Logical Plan).
 * 2. `ExploreGroup`: Рекурсивное применение `Transformation Rules` (напр. коммутативность Join).
 * 3. `OptimizeGroup`: Рекурсивное применение `Implementation Rules` (напр. Logical Join -> Hash Join)
 *    с одновременной оценкой стоимости I/O и CPU (Cost Estimation).
 * 4. Извлечение самого "дешевого" (Best Physical Plan) дерева физических операторов из Memo.
 */
class Optimizer {
public:
    Optimizer() {
        // Регистрация эвристик и рулсетов трансформации
        AddRule(std::make_unique<LogicalJoinCommutativityRule>());
        AddRule(std::make_unique<LogicalToPhysicalHashJoinRule>());
    }

    // Точка входа в CBO
    /*
    std::unique_ptr<PhysicalPlanNode> Optimize(std::unique_ptr<LogicalPlanNode> plan) {
        Memo memo;
        GroupId root_id = InjectLogicalPlanIntoMemo(plan.get(), &memo);
        
        OptimizeGroup(root_id, &memo);
        
        return ExtractBestPlan(root_id, &memo);
    }
    */

private:
   // void OptimizeGroup(GroupId group_id, Memo* memo);
   
   void AddRule(std::unique_ptr<Rule> rule) {
       rules_.push_back(std::move(rule));
   }

   std::vector<std::unique_ptr<Rule>> rules_;
};

} // namespace stingdb::planner