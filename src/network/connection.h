#pragma once

#include "system/instance.h"
#include "sql/binder.h"
#include "sql/ast.h"
#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <iostream>
#include <string_view>
#include <string>
#include <algorithm>

namespace stingdb::network {

using boost::asio::ip::tcp;

class Connection : public std::enable_shared_from_this<Connection> {
public:
    explicit Connection(tcp::socket socket, system::StingInstance* instance)
        : socket_(std::move(socket)), instance_(instance) {
    }

    void Start() {
        DoRead();
    }

private:
    void DoRead() {
        auto self(shared_from_this());
        socket_.async_read_some(boost::asio::buffer(data_, MAX_LENGTH),
            [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
                if (!ec) {
                    std::string_view query(data_, bytes_transferred);
                    std::string response;

                    try {
                        if (query.length() >= 12 && query.substr(0, 12) == "CREATE TABLE") {
                            std::vector<catalog::Column> cols;
                            cols.emplace_back("id", execution::TypeId::INTEGER, 4);
                            cols.emplace_back("name", execution::TypeId::VARCHAR, 255);

                            sql::CreateStatement create_stmt("users", std::move(cols));
                            sql::Binder binder(*instance_->GetCatalogManager());
                            binder.BindStatement(create_stmt);
                            
                            response = "Success: Table 'users' created.\n";
                        } else if (query.length() >= 5 && query.substr(0, 5) == "BEGIN") {
                            auto* txn = instance_->GetTransactionManager()->Begin(transaction::IsolationLevel::SNAPSHOT_ISOLATION);
                            response = "Success: Transaction " + std::to_string(txn->GetTxnId()) + " started.\n";
                        } else {
                            response = "Error: Unsupported statement. Try CREATE TABLE or BEGIN.\n";
                        }
                    } catch (const std::exception& e) {
                        response = std::string("Error: ") + e.what() + "\n";
                    }

                    DoWriteAnswer(std::move(response));
                }
            });
    }

    void DoWriteAnswer(std::string response) {
        auto self(shared_from_this());
        std::size_t len = std::min(response.length(), MAX_LENGTH);
        std::copy(response.begin(), response.begin() + static_cast<std::string::difference_type>(len), data_);

        boost::asio::async_write(socket_, boost::asio::buffer(data_, len),
            [this, self](boost::system::error_code ec, std::size_t /*bytes*/) {
                if (!ec) {
                    DoRead();
                } else {
                    socket_.close();
                }
            });
    }

    tcp::socket socket_;
    static constexpr std::size_t MAX_LENGTH = 8192;
    char data_[MAX_LENGTH]{};
    system::StingInstance* instance_;
};

} // namespace stingdb::network
