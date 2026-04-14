#pragma once

#include "common/config.h"
#include <atomic>
#include <shared_mutex>
#include <cstring>
#include <span>

namespace stingdb::storage {

/**
 * Класс Page представляет собой in-memory абстракцию дискового блока (размером PAGE_SIZE).
 * В производственном коде класс использует Read-Write Locks для консистентного доступа и атомики
 * для управления счетчиком пиннинга (pin_count) в многопоточной среде Buffer Pool'а.
 */
class Page {
public:
    Page() { ResetMemory(); }
    ~Page() = default;

    // Запрет копирования/перемещения (это закрепленный ресурс Buffer Pool'а)
    Page(const Page&) = delete;
    Page& operator=(const Page&) = delete;
    Page(Page&&) = delete;
    Page& operator=(Page&&) = delete;

    [[nodiscard]] inline char* GetData() { return data_; }
    [[nodiscard]] inline std::span<const char> GetSpan() const { 
        return std::span<const char>(data_, common::PAGE_SIZE); 
    }

    [[nodiscard]] inline common::PageId GetPageId() const { return page_id_.load(std::memory_order_acquire); }
    [[nodiscard]] inline int GetPinCount() const { return pin_count_.load(std::memory_order_acquire); }
    [[nodiscard]] inline bool IsDirty() const { return is_dirty_; }

    inline void SetDirty(bool is_dirty) { is_dirty_ = is_dirty; }

    inline void RLock() { rwlatch_.lock_shared(); }
    inline void RUnlock() { rwlatch_.unlock_shared(); }
    inline void WLock() { rwlatch_.lock(); }
    inline void WUnlock() { rwlatch_.unlock(); }
    
    inline void ResetMemory() { memset(data_, 0, common::PAGE_SIZE); }

private:
    friend class BufferPoolManager;

    alignas(16) char data_[common::PAGE_SIZE]{}; 
    std::atomic<common::PageId> page_id_{common::INVALID_PAGE_ID};
    std::atomic<int> pin_count_{0};
    bool is_dirty_{false};
    
    // std::shared_mutex может быть медленным под HighLoad, 
    // в будущем (Итерация 2) мы заменим это на spinlock с экспоненциальным backoff или reader-writer ticket lock.
    mutable std::shared_mutex rwlatch_;
};

} // namespace stingdb::storage