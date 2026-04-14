#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <stdexcept>
// Библиотека xsimd используется для переносимой SIMD-векторизации (SSE, AVX, NEON, SVE)
// Если она не установлена, сборка упадет с ошибкой (требуется ручная установка)
#include <xsimd/xsimd.hpp> 

namespace stingdb::execution {

enum class TypeId { INVALID = 0, BOOLEAN, INTEGER, BIGINT, FLOAT, DOUBLE, VARCHAR };

/**
 * Базовый класс колоночного вектора. 
 * Хранит логический тип данных и предоставляет виртуальный интерфейс
 * (при необходимости, однако в горячих циклах используется static_cast до FlatVector).
 */
class Vector {
public:
    explicit Vector(TypeId type) : type_(type) {}
    virtual ~Vector() = default;

    [[nodiscard]] TypeId GetType() const { return type_; }
    [[nodiscard]] virtual std::size_t GetSize() const = 0;
    
    // Битовая маска NULL значений (1 бит = 1 значение)
    [[nodiscard]] virtual const uint64_t* GetNullMask() const = 0;
    virtual void SetNull(std::size_t index, bool is_null) = 0;
    [[nodiscard]] virtual bool IsNull(std::size_t index) const = 0;

protected:
    TypeId type_;
};

/**
 * Плоский вектор данных (FlatVector).
 * Содержит массивы фиксированного размера, выровненные в памяти для SIMD инструкций.
 * Использует xsimd::default_allocator для обеспечения правильного alignment (например, 32 байта для AVX2).
 */
template<typename T>
class FlatVector : public Vector {
public:
    explicit FlatVector(TypeId type, std::size_t capacity = 1024) 
        : Vector(type), size_(0) {
        
        // Выравнивание массива под максимальную длину регистра целевой архитектуры
        data_.reserve(capacity);
        
        // Маска: 1 бит на элемент. Вычисляем количество uint64_t (блоков по 64 бита).
        std::size_t mask_words = (capacity + 63) / 64;
        null_mask_.assign(mask_words, 0); 
    }

    [[nodiscard]] std::size_t GetSize() const override { return size_; }
    void Resize(std::size_t new_size) { size_ = new_size; data_.resize(new_size); }

    [[nodiscard]] const T* GetData() const { return data_.data(); }
    T* GetDataMutable() { return data_.data(); }

    [[nodiscard]] const uint64_t* GetNullMask() const override { return null_mask_.data(); }

    // Установка NULL бита
    void SetNull(std::size_t index, bool is_null) override {
        std::size_t word_idx = index / 64;
        std::size_t bit_idx = index % 64;
        if (is_null) {
            null_mask_[word_idx] |= (1ULL << bit_idx);
        } else {
            null_mask_[word_idx] &= ~(1ULL << bit_idx);
        }
    }

    [[nodiscard]] bool IsNull(std::size_t index) const override {
        std::size_t word_idx = index / 64;
        std::size_t bit_idx = index % 64;
        return (null_mask_[word_idx] & (1ULL << bit_idx)) != 0;
    }

    // Для SIMD предикатов: загружаем батч элементов в векторный регистр
    // Пример: auto reg = vector.LoadSIMD(offset);
    inline auto LoadSIMD(std::size_t offset) const {
        return xsimd::load_aligned(data_.data() + offset);
    }

private:
    std::size_t size_;
    
    // Аллокатор xsimd гарантирует, что память будет выровнена (aligned) в зависимости
    // от инструкций, доступных на машине компиляции (AVX2/AVX512/NEON).
    std::vector<T, xsimd::default_allocator<T>> data_;
    
    // Вектор масок
    std::vector<uint64_t> null_mask_;
};

} // namespace stingdb::execution