#include <iostream>
#include <memory>
#include <thread>
#include <boost/asio.hpp>
#include "storage/disk/disk_manager.h"
#include "storage/buffer_pool/buffer_pool_manager.h"
#include "network/server.h"

int main() {
    try {
        std::cout << "[StingDB Server] Starting Production Engine Initialization..." << std::endl;

        // 1. Инициализация подсистемы Storage & I/O
        // O_DIRECT + io_uring активируются автоматически
        auto disk_manager = std::make_unique<stingdb::storage::DiskManager>("stingdb_data.db");
        
        // 2. Аллокация Buffer Pool'а. 
        // 4096 страниц по 8КБ = 32 Мегабайта оперативной памяти под кэш
        auto buffer_pool = std::make_unique<stingdb::storage::BufferPoolManager>(4096, disk_manager.get());
        std::cout << "[StingDB Server] Storage layer online. Buffer Pool allocated (32 MB)." << std::endl;

        // 3. Настройка TCP Реактора (Event-Loop) на порту 5432
        // Используем 5432 как задел для wire/socket совместимости с PostgreSQL
        boost::asio::io_context io_context;
        std::size_t hardware_threads = std::thread::hardware_concurrency();
        std::size_t pool_size = hardware_threads > 1 ? hardware_threads : 4;
        
        std::cout << "[StingDB Server] Initializing Async TCP Reactor on port 5432..." << std::endl;
        std::cout << "[StingDB Server] Thread Pool Size: " << pool_size << " workers." << std::endl;
        
        stingdb::network::Server server(io_context, 5432, pool_size);

        // 4. Запуск петли событий. Вызов Run() блокирует main-поток 
        // и диспетчеризует запросы по пулу потоков.
        std::cout << "[StingDB Server] Ready to accept connections. Waiting for queries..." << std::endl;
        server.Run();

    } catch (const std::exception& e) {
        std::cerr << "[StingDB FATAL] Engine aborted with exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}