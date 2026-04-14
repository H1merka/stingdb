#pragma once

#include "common/config.h"
#include "storage/page/page.h"
#include "storage/disk/disk_manager.h"

#include <vector>
#include <mutex>
#include <unordered_map>
#include <list>

namespace stingdb::storage {

/**
 * BufferPoolManager — это "мозг памяти" СУБД.
 * Предоставляет кэш между диском и оперативной памятью.
 * Производственная версия потребует алгоритма вытеснения (Clock/LRU-K/2Q).
 * 
 * В первой итерации этот класс определяет API вызовов для управления пинами.
 */
class BufferPoolManager {
public:
    BufferPoolManager(std::size_t pool_size, DiskManager* disk_manager);
    ~BufferPoolManager();

    BufferPoolManager(const BufferPoolManager&) = delete;
    BufferPoolManager& operator=(const BufferPoolManager&) = delete;

    // Взимает страницу с диска или возвращает из памяти. Увеличивает pin_count.
    Page* FetchPage(common::PageId page_id);
    
    // Освобождает страницу, счетчик pin_count уменьшается.
    bool UnpinPage(common::PageId page_id, bool is_dirty);
    
    // Создает новую физическую страницу на диске. Возвращает указатель на in-memory структуру с pin_count=1.
    Page* NewPage(common::PageId* first_page_id);
    
    // Удаляет страницу (и на диске и в пуле).
    bool DeletePage(common::PageId page_id);
    
    // Принудительно сбрасывает страницу на диск, независимо от pin_count.
    bool FlushPage(common::PageId page_id);
    
    // Сбрасывает все грязные страницы (используется при checkpointing/shutdown).
    void FlushAllPages();

private:
   // Глобальная блокировка пула. Для Production/HighLoad должна быть заменена
   // на массив независимых шардОВ (partitioning by mod pool_size).
    std::mutex latch_; 
    
    std::size_t pool_size_;
    Page* pages_; // Массив памяти Buffer Pool'a 
    DiskManager* disk_manager_;
    
    // Page table (маппинг ID страницы в ID фрейма пула)
    std::unordered_map<common::PageId, common::frame_id_t> page_table_;
    std::list<common::frame_id_t> lru_replacer_; // Временный LRU 
    std::list<common::frame_id_t> free_list_;
};

} // namespace stingdb::storage