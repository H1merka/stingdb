#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <utility>

namespace stingdb::common {

/**
 * Аренная аллокация для выполнения запросов.
 * Каждая Arena Allocator живет цикл жизни SQL-запроса и очищается мгновенно без free().
 * Исключает фрагментацию памяти и минимизирует системные вызовы аллокатора ОС.
 */
class ArenaAllocator {
public:
    explicit ArenaAllocator(std::size_t chunk_size = 4096)
        : chunk_size_(chunk_size), current_offset_(0) {
        AllocateChunk();
    }

    ~ArenaAllocator() = default;

    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;

    [[nodiscard]] void* Allocate(std::size_t size, std::size_t align = alignof(std::max_align_t)) {
        std::size_t padding = (align - (reinterpret_cast<std::uintptr_t>(chunks_.back().get()) + current_offset_) % align) % align;
        
        if (current_offset_ + size + padding > chunk_size_) {
            AllocateChunk();
            padding = 0; // Новый chunk уже выровнен.
        }

        void* ptr = chunks_.back().get() + current_offset_ + padding;
        current_offset_ += size + padding;
        return ptr;
    }

    // Полная очистка памяти без системного возврата ядру (O(1)).
    void Reset() {
        if (!chunks_.empty()) {
            auto first_chunk = std::move(chunks_.front());
            chunks_.clear();
            chunks_.push_back(std::move(first_chunk)); // Сохраняем хотя бы первый чанк для переиспользования.
        }
        current_offset_ = 0;
    }

private:
    void AllocateChunk() {
        chunks_.push_back(std::make_unique<char[]>(chunk_size_));
        current_offset_ = 0;
    }

    std::size_t chunk_size_;
    std::size_t current_offset_;
    std::vector<std::unique_ptr<char[]>> chunks_;
};

} // namespace stingdb::common