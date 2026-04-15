#pragma once

#include "catalog/column.h"
#include <vector>
#include <stdexcept>
#include <string_view>

namespace stingdb::catalog {

/**
 * Схема таблицы (Schema). 
 * Список колонок. Инициализируется при создании таблицы (CREATE TABLE)
 * и хранит вычислимые смещения (offsets) для семантической валидации (Binder'а).
 */
class Schema {
public:
    explicit Schema(std::vector<Column> columns) : columns_(std::move(columns)) {
        uint32_t current_offset = 0;
        
        for (auto& col : columns_) {
            col.SetOffset(current_offset);
            // Если тип переменной длины (Varchar), offset определяет смещение заголовка,
            // а длина хранится отдельно, но для простоты MVP мы резервируем fixed_length.
            current_offset += col.GetLength(); 
        }
        tuple_fixed_size_ = current_offset;
    }

    [[nodiscard]] const std::vector<Column>& GetColumns() const { return columns_; }
    [[nodiscard]] const Column& GetColumn(std::size_t index) const { return columns_[index]; }
    
    // Получение индекса колонки по ее имени (для семантической проверки SQL: SELECT name FROM users)
    [[nodiscard]] std::size_t GetColIdx(std::string_view col_name) const {
        for (std::size_t i = 0; i < columns_.size(); ++i) {
            if (columns_[i].GetName() == col_name) {
                return i;
            }
        }
        throw std::runtime_error("Column '" + std::string(col_name) + "' not found.");
    }

    [[nodiscard]] std::size_t GetColumnCount() const { return columns_.size(); }
    [[nodiscard]] uint32_t GetTupleFixedLength() const { return tuple_fixed_size_; }

private:
   std::vector<Column> columns_;
   uint32_t tuple_fixed_size_{0}; 
};

} // namespace stingdb::catalog