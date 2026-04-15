#pragma once

#include "sql/ast.h"
#include "catalog/catalog_manager.h"
#include "common/config.h"
#include <stdexcept>

namespace stingdb::sql {

/**
 * Валидатор и Связыватель (Binder). 
 * Преобразует сырое AST-дерево парсера в вызовы Системного Каталога, 
 * разрешая имена таблиц и столбцов.
 */
class Binder {
public:
    explicit Binder(catalog::CatalogManager& catalog) : catalog_(catalog) {}

    void BindStatement(const SQLStatement& stmt) {
        switch (stmt.GetType()) {
            case StatementType::CREATE_TABLE: {
                // Исключаем динамический каст (во избежание проблем с RTTI флагами)
                const auto& create_stmt = static_cast<const CreateStatement&>(stmt);
                BindCreateTable(create_stmt);
                break;
            }
            default:
                throw std::runtime_error("Unsupported statement type for binding");
        }
    }

private:
    void BindCreateTable(const CreateStatement& stmt) {
        // Заглушка первой страницы (обычно здесь DiskManager/BufferPool аллокирует новую страницу под Heap/TreeRoot).
        // Для MVP-Каталога инициализируем как INVALID_PAGE_ID
        auto initial_page_id = common::INVALID_PAGE_ID; 

        // Каталог самостоятельно вычислит смещения и длину внутри Schema
        catalog::Schema schema(stmt.columns);
        catalog_.CreateTable(stmt.table_name, schema, initial_page_id);
    }

    catalog::CatalogManager& catalog_;
};

} // namespace stingdb::sql