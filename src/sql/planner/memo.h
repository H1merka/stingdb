#pragma once

#include <vector>
#include <memory>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>

namespace stingdb::planner {

enum class OperatorType {
    LOGICAL_GET, LOGICAL_FILTER, LOGICAL_JOIN, LOGICAL_AGGREGATION,
    PHYSICAL_SEQ_SCAN, PHYSICAL_INDEX_SCAN, PHYSICAL_HASH_JOIN, PHYSICAL_NESTED_LOOP_JOIN
};

using GroupId = uint32_t;

/**
 * GroupExpression: Представляет собой единый план (логический или физический) внутри Группы.
 * Вместо прямых дочерних операторов здесь хранятся ID дочерних Групп (Group IDs), 
 * что делает граф планов - Memo (Направленный Ациклический Граф - DAG) очень компактным,
 * переиспользуя общие подзапросы.
 */
class GroupExpression {
public:
    GroupExpression(OperatorType op_type, std::vector<GroupId> children)
        : op_type_(op_type), children_(std::move(children)), hash_(ComputeHash()) {}

    [[nodiscard]] OperatorType GetOperatorType() const { return op_type_; }
    [[nodiscard]] const std::vector<GroupId>& GetChildren() const { return children_; }
    [[nodiscard]] std::size_t Hash() const { return hash_; }
    [[nodiscard]] GroupId GetGroupId() const { return target_group_id_; }
    void SetGroupId(GroupId id) { target_group_id_ = id; }

private:
   std::size_t ComputeHash() const {
       std::size_t h = std::hash<int>{}(static_cast<int>(op_type_));
       for (auto child : children_) {
           h ^= std::hash<GroupId>{}(child) + 0x9e3779b9 + (h << 6) + (h >> 2);
       }
       return h;
   }

   OperatorType op_type_;
   std::vector<GroupId> children_;
   std::size_t hash_;
   GroupId target_group_id_{0};
};

/**
 * Group: Контейнер всех логически эквивалентных (выдающих одинаковый результат) выражений.
 */
class Group {
public:
    explicit Group(GroupId id) : id_(id) {}

    void AddExpression(std::shared_ptr<GroupExpression> expr) {
        logical_expressions_.push_back(expr);
    }
    
    [[nodiscard]] GroupId GetId() const { return id_; }
    [[nodiscard]] const std::vector<std::shared_ptr<GroupExpression>>& GetLogicalExpressions() const { 
        return logical_expressions_; 
    }

private:
    GroupId id_;
    std::vector<std::shared_ptr<GroupExpression>> logical_expressions_;
    std::vector<std::shared_ptr<GroupExpression>> physical_expressions_;
    bool is_explored_{false};
};

/**
 * Memo-структура: Ядро Cost-Based Optimizer (Cascades).
 * Исключает дублирование планов при трансформациях благодаря хэшированию эквивалентных подзапросов.
 * Например: (A JOIN B) JOIN C и A JOIN (B JOIN C) будут по-максимуму повторно использовать Group'ы (TableGet A, B, C).
 */
class Memo {
public:
    Memo() = default;

    // Вставляет выражение в Memo, либо возвращает существующую Группу (если точная копия уже есть)
    GroupId InsertExpression(std::shared_ptr<GroupExpression> expr, GroupId target_group = 0) {
        if (auto it = expr_map_.find(expr->Hash()); it != expr_map_.end()) {
            return it->second->GetGroupId();
        }

        GroupId assigned_group = target_group;
        if (assigned_group == 0) {
            assigned_group = static_cast<GroupId>(groups_.size() + 1);
            groups_.push_back(std::make_shared<Group>(assigned_group));
        }
        
        expr->SetGroupId(assigned_group);
        expr_map_[expr->Hash()] = expr;
        groups_[assigned_group - 1]->AddExpression(expr);
        return assigned_group;
    }

private:
   std::vector<std::shared_ptr<Group>> groups_;
   std::unordered_map<std::size_t, std::shared_ptr<GroupExpression>> expr_map_;
};

} // namespace stingdb::planner