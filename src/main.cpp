#include <iostream>
#include <memory>
#include <thread>
#include <boost/asio.hpp>
#include "storage/disk/disk_manager.h"
#include "storage/buffer_pool/buffer_pool_manager.h"
#include "network/server.h"
#include "system/instance.h"

int main() {
    try {
        std::cout << "[StingDB Server] Starting Production Engine Initialization..." << std::endl;

        // 1. Инициализация подсистемы Storage & I/O
        auto disk_manager = std::make_unique<stingdb::storage::DiskManager>("stingdb_data.db");

        // 2. Аллокация Buffer Pool'а.
        auto buffer_pool = std::make_unique<stingdb::storage::BufferPoolManager>(4096, disk_manager.get());
        std::cout << "[StingDB Server] Storage layer online. Buffer Pool allocated (32 MB)." << std::endl;

        // 3. Инициализация глобального контекста СУБД (Каталог, Транзакции)
        auto instance = std::make_unique<stingdb::system::StingInstance>(buffer_pool.get());
        std::cout << "[StingDB Server] Core Subsystems (Catalog, TXN) initialized." << std::endl;

        // 4. Настройка TCP Реактора (Event-Loop) на порту 5432
        boost::asio::io_context io_context;
        std::size_t hardware_threads = std::thread::hardware_concurrency();     
        std::size_t pool_size = hardware_threads > 1 ? hardware_threads : 4;    

        std::cout << "[StingDB Server] Initializing Async TCP Reactor on port 5432..." << std::endl;
        std::cout << "[StingDB Server] Thread Pool Size: " << pool_size << " workers." << std::endl;

        stingdb::network::Server server(io_context, 5432, pool_size, instance.get());

        // 5. Запуск петли событий
        std::cout << "[StingDB Server] Ready to accept connections. Waiting for queries..." << std::endl;
        server.Run();

    } catch (const std::exception& e) {
        std::cerr << "[StingDB FATAL] Engine aborted with exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
