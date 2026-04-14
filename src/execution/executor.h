#pragma once

#include "execution/tuple_batch.h"
#include <memory>

namespace stingdb::execution {

/**
 * Базовый интерфейс оператора для Vectorized Execution Model.
 * В отличие от старого Volcano (где Next() возвращает 1 Tuple),
 * здесь `Next()` возвращает батч колонок (`TupleBatch`).
 */
class ExecutionOperator {
public:
    virtual ~ExecutionOperator() = default;

    virtual void Init() = 0;
    
    // Возвращает `true`, если батч сформирован, и `false`, если данных больше нет.
    virtual bool Next(TupleBatch* batch_out) = 0;
};

/**
 * Пример сканирования (SeqScan) в векторизованной парадигме.
 * 
 * В продакшене `SeqScanExecutor` извлекает `Tuple` из `SlottedPage` (хранящегося по строкам - N-ary Storage Model),
 * и сразу "расщепляет" их в колонки (Projection) внутри `TupleBatch`.
 */
class SeqScanExecutor : public ExecutionOperator {
public:
    explicit SeqScanExecutor() {}

    void Init() override {
        // ... (инициализация scan context'а над таблицей)
    }

    bool Next(TupleBatch* batch_out) override {
        // 1. Увеличиваем размер батчей до `DEFAULT_BATCH_SIZE`.
        // 2. Читаем кортежи (Tuples) через `BufferPoolManager`.
        // 3. Если сканируем колонку с типом INTEGER (например age >= 18), 
        //    мы можем загрузить xsimd::batch<int32_t> и мгновенно (за 1 такт CPU) отфильтровать 8/16 значений.
        return false; // Заглушка
    }
};

/**
 * Пример оператора фильтрации с SIMD
 */
class FilterExecutor : public ExecutionOperator {
public:
    FilterExecutor(std::unique_ptr<ExecutionOperator> child) : child_(std::move(child)) {}

    void Init() override {
        child_->Init();
    }

    bool Next(TupleBatch* batch_out) override {
        TupleBatch input_batch;
        
        while (child_->Next(&input_batch)) {
            // Например, фильтруем столбец 0 (возрасты)
            /*
            auto* col = static_cast<FlatVector<int32_t>*>(input_batch.GetColumn(0).get());
            
            // Векторизация `xsimd` (один проход по 8/16 элементов).
            // Компилятор сгенерирует здесь vcmpgtps/vpblendvb (AVX)
            std::size_t i = 0;
            constexpr std::size_t batch_size = xsimd::batch<int32_t>::size; // 8 элементов в AVX2

            for (; i + batch_size <= input_batch.GetNumRows(); i += batch_size) {
                auto vals = xsimd::load_aligned(col->GetData() + i);
                auto threshold = xsimd::batch<int32_t>(18);
                auto mask = vals >= threshold; // SIMD сравнение (за один инструкционный такт)

                // Формируем Selection Vector
                // ... (игнорируем или копируем прошедшие предикат строки)
            }
            */

             return true; // Возвращаем отфильтрованный батч
        }
        return false;
    }

private:
   std::unique_ptr<ExecutionOperator> child_;
};

} // namespace stingdb::execution