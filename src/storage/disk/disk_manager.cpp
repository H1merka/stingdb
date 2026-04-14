#include "storage/disk/disk_manager.h"
#include <stdexcept>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

namespace stingdb::storage {

DiskManager::DiskManager(std::string_view db_file) : file_name_(db_file) {
    // В Production требуется использование флага O_DIRECT для обхода Page Cache ОС
    // Но O_DIRECT также требует выравнивания буферов в памяти, чего мы добьемся в будущем через posix_memalign
    fd_ = open(file_name_.c_str(), O_RDWR | O_CREAT | O_DIRECT, 0666);
    if (fd_ < 0) {
        // Fallback если файловая система не поддерживает O_DIRECT в WSL 
        fd_ = open(file_name_.c_str(), O_RDWR | O_CREAT, 0666);
        if (fd_ < 0) {
            throw std::runtime_error("DiskManager: failed to open db file");
        }
    }

    // Инициализация io_uring. 256 entries.
    // Параметр 0 означает default behaviour.
    if (io_uring_queue_init(256, &ring_, 0) < 0) {
        throw std::runtime_error("DiskManager: failed to initialize io_uring");
    }
}

DiskManager::~DiskManager() {
    io_uring_queue_exit(&ring_);
    close(fd_);
}

void DiskManager::ReadPage(common::PageId page_id, char* page_data) {
    off_t offset = static_cast<off_t>(page_id) * static_cast<off_t>(common::PAGE_SIZE);
    
    // Синхронное чтение через pread (thread-safe альтернатива lseek + read).
    // Позволяет читать нескольким потокам одновременно из одного fd.
    ssize_t bytes_read = pread(fd_, page_data, common::PAGE_SIZE, offset);
    
    if (bytes_read < 0) {
        throw std::runtime_error("DiskManager: failed to read page");
    }
    
    // Если прочитано меньше PAGE_SIZE (например, новая база), заполняем нулями.
    if (bytes_read < static_cast<ssize_t>(common::PAGE_SIZE)) {
        memset(page_data + bytes_read, 0, common::PAGE_SIZE - static_cast<size_t>(bytes_read));
    }
}

void DiskManager::WritePageAsync(common::PageId page_id, const char* page_data) {
    struct io_uring_sqe* sqe = io_uring_get_sqe(&ring_);
    if (!sqe) {
        throw std::runtime_error("DiskManager: io_uring submission queue is full");
    }

    off_t offset = static_cast<off_t>(page_id) * static_cast<off_t>(common::PAGE_SIZE);

    // Подготовка асинхронной записи.
    // Важно: в реальном production для io_uring нужен буфер page_data,
    // который доживет до момента завершения I/O (CQE).
    io_uring_prep_write(sqe, fd_, page_data, common::PAGE_SIZE, static_cast<__u64>(offset));
    
    // Передача контекста (например, указателя или ID) для обработки в RetriveAsyncCompletions.
    // Здесь мы просто ставим page_id как user_data.
    io_uring_sqe_set_data64(sqe, static_cast<__u64>(page_id));

    io_uring_submit(&ring_);
}

int DiskManager::RetrieveAsyncCompletions() {
    struct io_uring_cqe* cqe;
    int completions = 0;
    
    // Non-blocking проверка (peek) очереди завершенных задач.
    while (io_uring_peek_cqe(&ring_, &cqe) == 0) {
        if (cqe->res < 0) {
            std::cerr << "DiskManager: async write failed with error code: " << cqe->res << std::endl;
        }

        // common::PageId page_id = static_cast<common::PageId>(io_uring_cqe_get_data64(cqe));
        // TODO: здесь можно сообщить Buffer Pool'у, что грязная страница наконец сброшена.
        
        io_uring_cq_advance(&ring_, 1);
        completions++;
    }
    return completions;
}

} // namespace stingdb::storage
