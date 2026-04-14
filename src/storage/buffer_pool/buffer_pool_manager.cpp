#include "storage/buffer_pool/buffer_pool_manager.h"
#include <iostream>
#include <cassert>

namespace stingdb::storage {

BufferPoolManager::BufferPoolManager(std::size_t pool_size, DiskManager* disk_manager) 
    : pool_size_(pool_size), disk_manager_(disk_manager) {
    
    // В Production для O_DIRECT требуется posix_memalign или std::aligned_alloc
    // с выравниванием по размеру сектора диска (4096).
    // Мы эмулируем массив Page:
    pages_ = static_cast<Page*>(::operator new[](pool_size_ * sizeof(Page), std::align_val_t(4096)));
    for (std::size_t i = 0; i < pool_size_; ++i) {
        new (&pages_[i]) Page();
        free_list_.push_back(static_cast<common::frame_id_t>(i));
    }
}

BufferPoolManager::~BufferPoolManager() {
    FlushAllPages();

    for (std::size_t i = 0; i < pool_size_; ++i) {
        pages_[i].~Page();
    }
    ::operator delete[](pages_, std::align_val_t(4096));
}

Page* BufferPoolManager::FetchPage(common::PageId page_id) {
    std::lock_guard<std::mutex> lock(latch_);
    
    // 1. Поиск страницы в пуле (уже загружена).
    auto it = page_table_.find(page_id);
    if (it != page_table_.end()) {
        common::frame_id_t frame_id = it->second;
        Page* page = &pages_[frame_id];
        
        // Увеличение pin_count блокирует вытеснение страницы алгоритмом LRU-K
        page->pin_count_.fetch_add(1, std::memory_order_relaxed);
        
        // Извлечение из LRU списка (страница используется - не кандидат на удаление)
        lru_replacer_.remove(frame_id); 
        
        return page;
    }

    // 2. Иначе, страница на диске. Ищем свободный фрейм.
    common::frame_id_t frame_id = -1;
    if (!free_list_.empty()) {
        frame_id = free_list_.front();
        free_list_.pop_front();
    } else if (!lru_replacer_.empty()) {
        // Вытеснение с помощью LRU
        frame_id = lru_replacer_.back();
        lru_replacer_.pop_back();

        Page* victim_page = &pages_[frame_id];
        if (victim_page->IsDirty()) {
            std::cerr << "Sync write on Eviction for Page: " << victim_page->GetPageId() << " (Spike/Stall risk)" << std::endl;
            // Для упрощения: синхронный сброс. В будущем здесь io_uring или вызов Flusher Thread.
            // disk_manager_->WritePageAsync(victim_page->GetPageId(), victim_page->GetData());
            disk_manager_->WritePageAsync(victim_page->GetPageId(), victim_page->GetData()); 
            // Ждем завершения простого I/O (неблокирующе/или блокирующе)
            while(disk_manager_->RetrieveAsyncCompletions() == 0){}
        }
        
        page_table_.erase(victim_page->GetPageId());
    } else {
        // OOM в BufferPool - нет свободных страниц и ничего нельзя удалить 
        // (все закреплено работающими запросами).
        return nullptr; 
    }

    // 3. Загружаем страницу с диска.
    Page* page = &pages_[frame_id];
    page->ResetMemory();
    disk_manager_->ReadPage(page_id, page->GetData());
    
    // 4. Обновляем метаданные
    page->page_id_.store(page_id, std::memory_order_relaxed);
    page->pin_count_.store(1, std::memory_order_relaxed);
    page->SetDirty(false);
    
    page_table_[page_id] = frame_id;

    // В LRU список страницу не добавляем, так как pin_count = 1 
    return page;
}

bool BufferPoolManager::UnpinPage(common::PageId page_id, bool is_dirty) {
    std::lock_guard<std::mutex> lock(latch_);
    
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) return false;
    
    common::frame_id_t frame_id = it->second;
    Page* page = &pages_[frame_id];
    
    if (is_dirty) {
        page->SetDirty(true);
    }
    
    int previous_pin_count = page->pin_count_.fetch_sub(1, std::memory_order_relaxed);
    assert(previous_pin_count > 0 && "Unpinning an unpinned page.");
    
    // Если счетчик достигает нуля, страница может быть вытеснена в будущем (кандидат)
    if (previous_pin_count == 1) {
        // Добавляем в голову LRU как "недавно использованную", но доступную для вытеснения
        lru_replacer_.push_front(frame_id);
    }
    
    return true;
}

bool BufferPoolManager::FlushPage(common::PageId page_id) {
    std::lock_guard<std::mutex> lock(latch_);
    auto it = page_table_.find(page_id);
    if (it == page_table_.end()) return false;

    common::frame_id_t frame_id = it->second;
    Page* page = &pages_[frame_id];
    
    if (page->IsDirty()) {
        disk_manager_->WritePageAsync(page_id, page->GetData()); 
        page->SetDirty(false);
    }
    return true;
}

void BufferPoolManager::FlushAllPages() {
    std::lock_guard<std::mutex> lock(latch_);
    for (auto const& [page_id, frame_id] : page_table_) {
        Page* page = &pages_[frame_id];
        if (page->IsDirty()) {
             // Production warning: асинхронная групповая запись должна быть здесь
             disk_manager_->WritePageAsync(page_id, page->GetData());
             page->SetDirty(false);
        }
    }
    // Форс-дожидание всех асинхронных операций
    while(disk_manager_->RetrieveAsyncCompletions() < 0){} // Эмуляция спин-лока для демо-shutdown
}

Page* BufferPoolManager::NewPage(common::PageId* first_page_id) {
    // В будущем здесь будет обращение к Дисковому Менеджеру за следующим свободным ID из метаданных.
    // Для Итерации 1 возвращаем nullptr (пока не написан Каталог Метаданных Файла).
    return nullptr;
}

bool BufferPoolManager::DeletePage(common::PageId page_id) {
    // Реализация освобождения на диске и в пуле (оставлено под Итерацию 2)
    return false;
}

} // namespace stingdb::storage