#pragma once

#include "catalog/column.h"
#include <string>
#include <vector>
#include <utility>

namespace stingdb::sql {

enum class StatementType { CREATE_TABLE, INSERT, SELECT };

/**
 * Базовый класс для всех SQL-выражений (AST).
 */
struct SQLStatement {
    virtual ~SQLStatement() = default;
    [[nodiscard]] virtual StatementType GetType() const = 0;
};

/**
 * AST-узел для выражения CREATE TABLE.
 */
struct CreateStatement : public SQLStatement {
    std::string table_name;
    std::vector<catalog::Column> columns;

    CreateStatement(std::string name, std::vector<catalog::Column> cols)
        : table_name(std::move(name)), columns(std::move(cols)) {}

    [[nodiscard]] StatementType GetType() const override { return StatementType::CREATE_TABLE; }
};

} // namespace stingdb::sql