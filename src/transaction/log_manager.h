#pragma once

#include "transaction/log_record.h"
#include <mutex>
#include <condition_variable>

namespace stingdb::transaction {

/**
 * LogManager: Буфер WAL (Write-Ahead Log).
 * Отвечает за прием физиологических логов от Worker-потоков и
 * групповую отправку (Group Commit) их на диск посредством io_uring.
 */
class LogManager {
public:
    LogManager() : log_buffer_(common::PAGE_SIZE * 2), flush_buffer_(common::PAGE_SIZE * 2) {
        // Запуск фонового потока (Flusher), который каждые 1-5мс дергает Flush.
        // Здесь мы пока ограничиваемся синхронным интерфейсом.
    }

    ~LogManager() = default;

    // Никто не ждет диска здесь — запись ложится в in-memory буфер
    TypeEpoch AppendLogRecord(LogRecord* log_record) {
        std::lock_guard<std::mutex> lock(latch_);
        
        // В продакшене: Если буфер заполнен > 70%, 
        // посылаем сигнал фоновому потоку на Group Commit:
        if (log_buffer_offset_ + log_record->GetSize() > log_buffer_.size()) {
            SwapBuffers();
            cv_.notify_one(); // Пробуждение I/O-треда для Flush
        }

        // Сериализуем log_record в log_buffer_ (копируем память)
        // ... (memcpy data) ...
        log_buffer_offset_ += log_record->GetSize();
        
        // LSN - Логический Порядковый Номер записи (Log Sequence Number)
        // для нас - просто оффсет байтов глобально или атомарный счетчик.
        return next_lsn_.fetch_add(1, std::memory_order_relaxed);
    }

    // Принудительный сброс на диск (вызывается на COMMIT транзакции)
    void Flush() {
        std::unique_lock<std::mutex> lock(latch_);
        SwapBuffers();
        // Освобождаем лок, пока идет I/O (чтобы не блочить инсерты других транзакций)
        lock.unlock();

        // disk_manager_->WriteLogAsync(flush_buffer_, flush_size_);
        // disk_manager_->RetrieveAsyncCompletions();
        flush_size_ = 0; // Буфер сброшен
    }

private:
    void SwapBuffers() {
        std::swap(log_buffer_, flush_buffer_);
        flush_size_ = log_buffer_offset_;
        log_buffer_offset_ = 0;
    }

    std::mutex latch_;
    std::condition_variable cv_;
    
    // Два буфера (Ping-Pong). В один пишут воркеры, другой в этот момент сбрасывается io_uring'ом
    std::vector<char> log_buffer_;
    std::vector<char> flush_buffer_;
    std::size_t log_buffer_offset_{0};
    std::size_t flush_size_{0};
    
    std::atomic<TypeEpoch> next_lsn_{1}; // 0 - INVALID LSN
};

} // namespace stingdb::transaction