#pragma once

#include <boost/asio.hpp>
#include <memory>
#include <iostream>
#include <string_view>

namespace stingdb::network {

using boost::asio::ip::tcp;

/**
 * Класс Connection: Неблокирующее обслуживание единичного клиентского соединения.
 * Использование std::enable_shared_from_this гарантирует, что объект Connection
 * будет жить в памяти ровно до тех пор, пока на него есть незавершенные (pending)
 * асинхронные I/O операции в Event Loop (io_context).
 */
class Connection : public std::enable_shared_from_this<Connection> {
public:
    explicit Connection(tcp::socket socket)
        : socket_(std::move(socket)) {
    }

    // Запуск прослушивания клиентских данных
    void Start() {
        DoRead();
    }

private:
    void DoRead() {
        auto self(shared_from_this()); // Захват life-cycle для лямбды
        socket_.async_read_some(boost::asio::buffer(data_, MAX_LENGTH),
            [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
                if (!ec) {
                    // Здесь в Production мы передаем полученную строку (data_) 
                    // нашему Parser -> Planner -> Optimizer -> Executor.
                    // Сейчас (для Итерации 6) делаем простую эхо-заглушку в виде ответа,
                    // чтобы протестировать пропускную способность сети.
                    
                    std::string_view query(data_, bytes_transferred);
                    // stingdb::sql::Parser parser(query); ...
                    
                    // Отправляем ответ клиенту неблокирующе
                    DoWrite(bytes_transferred);
                }
            });
    }

    // Отправка результата (Result Set) обратно клиенту
    void DoWrite(std::size_t length) {
        auto self(shared_from_this());
        boost::asio::async_write(socket_, boost::asio::buffer(data_, length),
            [this, self](boost::system::error_code ec, std::size_t /*bytes_transferred*/) {
                if (!ec) {
                    // Цикл: возвращаемся к чтению нового SQL-запроса в этом же соединении
                    DoRead();
                } else {
                    // Клиент отключился или произошла ошибка
                    socket_.close();
                }
            });
    }

    tcp::socket socket_;
    static constexpr std::size_t MAX_LENGTH = 8192; // 8 KB буфер команды (помещается в L1)
    char data_[MAX_LENGTH]{};
};

} // namespace stingdb::network