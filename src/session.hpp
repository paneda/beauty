#pragma once
// clang-format off
#include "environment.hpp"
// clang-format on

#include "asio.hpp"

class session : public std::enable_shared_from_this<session> {
   public:
    session(asio::ip::tcp::socket socket) : socket_(std::move(socket)) {}

    void start() {
        doRead();
    }

   private:
    void doRead() {
        auto self(shared_from_this());
        socket_.async_read_some(asio::buffer(data_, max_length),
                                [this, self](std::error_code ec, std::size_t length) {
                                    if (!ec) {
                                        data_[length] = 0;
                                        doWrite(length);
                                    }
                                });
    }

    void doWrite(std::size_t length) {
        auto self(shared_from_this());
        asio::async_write(socket_,
                          asio::buffer(data_, length),
                          [this, self](std::error_code ec, std::size_t length) {
                              if (!ec) {
                                  doRead();
                              }
                          });
    }

    asio::ip::tcp::socket socket_;
    enum { max_length = 1024 };
    char data_[max_length];
};
