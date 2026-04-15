#pragma once

#include "common/config.h"
#include <string_view>
#include <string>
#include <atomic>
#include <liburing.h> 

namespace stingdb::storage {

class DiskManager {
public:
    explicit DiskManager(std::string_view db_file);
    ~DiskManager();

    DiskManager(const DiskManager&) = delete;
    DiskManager& operator=(const DiskManager&) = delete;

    /**
     * Синхронное чтение страницы. Для фоновых воркеров или cold cache.
     */
    void ReadPage(common::PageId page_id, char* page_data);
    
    /**
     * Асинхронная запись с использованием io_uring (Production I/O).
     * Разгружаем основной тред выполнения. Используется фоновым потоком сброса грязных страниц (Flusher).
     */
    void WritePageAsync(common::PageId page_id, const char* page_data);

    /**
     * Обработка завершения всех асинхронных операций io_uring.
     * Возвращает кол-во обработанных Completion Queue Entries (CQE).
     */
    int RetrieveAsyncCompletions();

    /**
     * Выделяет новый идентификатор страницы.
     */
    common::PageId AllocatePage() {
        return next_page_id_.fetch_add(1);
    }

private:
    std::string file_name_;
    int fd_;
    std::atomic<common::PageId> next_page_id_{0};

    // Экземпляр кольцевого буфера io_uring.
    // Обязателен размер степени двойки (например, 1024 или 4096).
    struct io_uring ring_{};
};

} // namespace stingdb::storage