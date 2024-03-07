#include "connection.hpp"

#include <iostream>
#include <utility>
#include <vector>

#include "connection_manager.hpp"
#include "request_handler.hpp"

namespace http {
namespace server {

Connection::Connection(asio::ip::tcp::socket socket,
                       ConnectionManager &manager,
                       RequestHandler &handler,
                       unsigned connectionId)
    : socket_(std::move(socket)),
      connectionManager_(manager),
      requestHandler_(handler),
      connectionId_(connectionId) {}

void Connection::start() {
    doRead();
}

void Connection::stop() {
    socket_.close();
}

void Connection::doRead() {
    auto self(shared_from_this());
    socket_.async_read_some(
        asio::buffer(buffer_), [this, self](std::error_code ec, std::size_t bytes_transferred) {
            if (!ec) {
                RequestParser::result_type result;
                std::tie(result, std::ignore) = requestParser_.parse(
                    request_, buffer_.data(), buffer_.data() + bytes_transferred);

                if (result == RequestParser::good) {
                    requestHandler_.handleRequest(connectionId_, request_, reply_);
                    doWriteHeaders();
                } else if (result == RequestParser::bad) {
                    reply_ = Reply::stockReply(Reply::bad_request);
                    doWriteHeaders();
                } else {
                    doRead();
                }
            } else if (ec != asio::error::operation_aborted) {
                std::cout << "doRead: " << ec.message() << ':' << ec.value() << std::endl;
                connectionManager_.stop(shared_from_this());
            }
        });
}

void Connection::doWriteHeaders() {
    auto self(shared_from_this());
    asio::async_write(
        socket_, reply_.headerToBuffers(), [this, self](std::error_code ec, std::size_t) {
            if (!ec) {
                doWriteContent();
            } else {
                std::cout << "doWriteHeaders: " << ec.message() << ':' << ec.value() << std::endl;
                std::error_code ignored_ec;
                socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored_ec);
                connectionManager_.stop(shared_from_this());
                requestHandler_.closeFile(connectionId_);
            }
        });
}

void Connection::doWriteContent() {
    auto self(shared_from_this());
    asio::async_write(
        socket_, reply_.contentToBuffers(), [this, self](std::error_code ec, std::size_t) {
            if (!ec) {
                if (reply_.useChunking_) {
                    if (reply_.finalChunk_) {
                        // Initiate graceful connection closure.
                        std::error_code ignored_ec;
                        socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored_ec);
                        connectionManager_.stop(shared_from_this());
                    } else {
                        requestHandler_.handleChunk(connectionId_, reply_);
                        doWriteContent();
                    }
                } else {
                    // initiate graceful connection closure.
                    std::error_code ignored_ec;
                    socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored_ec);
                    connectionManager_.stop(shared_from_this());
                }
            } else {
                std::cout << "doWriteContent: " << ec.message() << ':' << ec.value() << std::endl;
                std::error_code ignored_ec;
                socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored_ec);
                connectionManager_.stop(shared_from_this());
                requestHandler_.closeFile(connectionId_);
            }
        });
}

}  // namespace server
}  // namespace http
