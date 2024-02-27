#include "connection.hpp"

#include <utility>
#include <vector>

#include "connection_manager.hpp"
#include "request_handler.hpp"

namespace http {
namespace server {

Connection::Connection(asio::ip::tcp::socket socket,
                       ConnectionManager &manager,
                       RequestHandler &handler)
    : socket_(std::move(socket)), connectionManager_(manager), requestHandler_(handler) {}

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
                    requestHandler_.handleRequest(request_, reply_);
                    doWrite();
                } else if (result == RequestParser::bad) {
                    reply_ = Reply::stockReply(Reply::bad_request);
                    doWrite();
                } else {
                    doRead();
                }
            } else if (ec != asio::error::operation_aborted) {
                connectionManager_.stop(shared_from_this());
            }
        });
}

void Connection::doWrite() {
    auto self(shared_from_this());
    asio::async_write(socket_, reply_.toBuffers(), [this, self](std::error_code ec, std::size_t) {
        if (!ec) {
            // Initiate graceful connection closure.
            std::error_code ignored_ec;
            socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored_ec);
        }

        if (ec != asio::error::operation_aborted) {
            connectionManager_.stop(shared_from_this());
        }
    });
}

}  // namespace server
}  // namespace http
