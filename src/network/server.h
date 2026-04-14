#pragma once

#include "network/connection.h"
#include <boost/asio.hpp>
#include <thread>
#include <vector>

namespace stingdb::network {

using boost::asio::ip::tcp;

/**
 * Server: Главный Реактор (Event-Loop) на базе `epoll` через Boost.Asio.
 * Поддерживает архитектуру "Один io_context - N потоков",
 * которая является образцом (industry standard) для HighLoad C++ сервисов:
 * Все TCP-сокеты прослушиваются в едижной очереди событий ОС, а обработка
 * запросов диспетчеризируется на фиксированный пул рабочих потоков.
 */
class Server {
public:
    // Инициализация TCP прослушивателя на порту (например, 5432) 
    // с заданным размером пула потоков (обычно = std::thread::hardware_concurrency()).
    Server(boost::asio::io_context& io_context, short port, std::size_t thread_pool_size)
        : io_context_(io_context),
          acceptor_(io_context, tcp::endpoint(tcp::v4(), port)),
          thread_pool_size_(thread_pool_size) {
        
        // Разрешаем переиспользование порта для быстрого рестарта.
        boost::asio::socket_base::reuse_address option(true);
        acceptor_.set_option(option);

        // Инициируем петлю приема соединений.
        DoAccept();
    }

    // Блокирующий метод, запускает размножение петли (io_context_.run())
    // по рабочим потокам для масштабируемости (Thread Pool for Event Loop).
    void Run() {
        std::vector<std::thread> threads;
        for (std::size_t i = 0; i < thread_pool_size_; ++i) {
            threads.emplace_back([this]() {
                io_context_.run();
            });
        }
        
        // Главный thread присоединяется ко всем рабочим
        for (auto& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }
    }

private:
    void DoAccept() {
        // Как только новое TCP-соединение установлено ядром, мы получаем control
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    // Создаем изолированную сессию (Connection) и стартуем ее обсчет
                    std::make_shared<Connection>(std::move(socket))->Start();
                }
                
                // Бесконечный цикл - снова переходим в ожидание нового подключения
                DoAccept();
            });
    }

    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
    std::size_t thread_pool_size_;
};

} // namespace stingdb::network