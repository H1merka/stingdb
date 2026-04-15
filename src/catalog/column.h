#pragma once

#include "execution/vector.h"
#include <string>
#include <cstdint>

namespace stingdb::catalog {

/**
 * Описание колонки (Column) базы данных.
 * Инкапсулирует тип данных, имя и фиксированную длину (например, для типов INTEGER, FLOAT), 
 * или лимит длины (для VARCHAR). Смещения (offsets) вычисляет Schema.
 */
class Column {
public:
    Column(std::string name, execution::TypeId type, uint32_t length = 0)
        : name_(std::move(name)), type_(type), length_(length) {
        
        // Автоматический расчет длины для примитивов (чтобы не заставлять пользователя)
        if (length == 0) {
            switch (type_) {
                case execution::TypeId::BOOLEAN: length_ = 1; break;
                case execution::TypeId::INTEGER: length_ = 4; break;
                case execution::TypeId::BIGINT:
                case execution::TypeId::DOUBLE:  length_ = 8; break;
                case execution::TypeId::FLOAT:   length_ = 4; break;
                case execution::TypeId::VARCHAR: length_ = 255; break; // Default max size
                default: length_ = 0; break;
            }
        }
    }

    [[nodiscard]] const std::string& GetName() const { return name_; }
    [[nodiscard]] execution::TypeId GetType() const { return type_; }
    [[nodiscard]] uint32_t GetLength() const { return length_; }
    
    // Внутреннее смещение в физическом кортеже (Tuple)
    [[nodiscard]] uint32_t GetOffset() const { return offset_; }
    void SetOffset(uint32_t offset) { offset_ = offset; }

private:
    std::string name_;
    execution::TypeId type_;
    uint32_t length_;
    uint32_t offset_{0};
};

} // namespace stingdb::catalog