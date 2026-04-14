#pragma once

#include "execution/vector.h"
#include <memory>
#include <vector>
#include <cstdint>

namespace stingdb::execution {

/**
 * TupleBatch (он же RecordBatch / VectorBatch).
 * Представляет собой набор (batch) строк, организованный колоночно.
 * В Vectorized Execution Engine (VEE) операторы обмениваются батчами,
 * минимизируя накладные расходы на виртуальные вызовы `Next()`.
 * Размер батча обычно кратен размеру кэш-линии / SIMD вектора, обычно 1024-4096.
 */
class TupleBatch {
public:
    static constexpr std::size_t DEFAULT_BATCH_SIZE = 1024;

    TupleBatch() : num_rows_(0) {}
    explicit TupleBatch(std::size_t initial_columns) : num_rows_(0), columns_(initial_columns) {}

    // Добавление вектора в батч.
    void AddColumn(std::shared_ptr<Vector> column) {
        columns_.push_back(std::move(column));
    }

    [[nodiscard]] std::size_t GetNumRows() const { return num_rows_; }
    void SetNumRows(std::size_t rows) { num_rows_ = rows; }

    [[nodiscard]] std::size_t GetNumColumns() const { return columns_.size(); }

    // Получение конкретной колонки с кастомным `static_cast` в вызывающей функции.
    [[nodiscard]] std::shared_ptr<Vector> GetColumn(std::size_t index) const {
        return columns_[index];
    }
    
    // Сброс (с сохранением выделенной памяти векторов).
    void Reset() {
        num_rows_ = 0;
    }

private:
    std::size_t num_rows_;
    // Каждый столбец представляется отдельным вектором.
    std::vector<std::shared_ptr<Vector>> columns_;
};

} // namespace stingdb::execution