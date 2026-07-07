#pragma once
#include "beauty/environment.hpp"

#include <asio.hpp>
#include <functional>
#include <memory>
#include <vector>

namespace beauty {

// Type-erased socket interface. Wraps either a plain TCP socket or an SSL
// stream so that Connection, HttpClient, and WsClient can operate on both
// without being templates.
class ISocket {
   public:
    using IoHandler = std::function<void(const std::error_code&, std::size_t)>;
    using ErrorHandler = std::function<void(const std::error_code&)>;
    using ConnectHandler =
        std::function<void(const std::error_code&, const asio::ip::tcp::endpoint&)>;

    virtual ~ISocket() = default;

    // Asynchronous read into a contiguous mutable buffer.
    virtual void asyncReadSome(const asio::mutable_buffer& buffer, IoHandler handler) = 0;

    // Asynchronous write of a buffer sequence.
    virtual void asyncWrite(const std::vector<asio::const_buffer>& buffers, IoHandler handler) = 0;

    // Close the underlying socket.
    virtual void close() = 0;

    // Graceful TCP shutdown.
    virtual void shutdown(std::error_code& ec) = 0;

    // Whether the underlying socket is open.
    virtual bool isOpen() const = 0;

    // Whether a TLS handshake is required before I/O. PlainSocket returns
    // false; SslSocket returns true.
    virtual bool needsHandshake() const = 0;

    // Asynchronous connect (client-side).
    virtual void asyncConnect(const asio::ip::tcp::resolver::results_type& endpoints,
                              ConnectHandler handler) = 0;

    // Perform an SSL/TLS handshake. For plain sockets this calls the handler
    // synchronously with a success error_code (no-op).
    virtual void asyncHandshake(bool isServer, ErrorHandler handler) = 0;

    // Access the underlying TCP socket (for accept, socket options, etc.).
    virtual asio::ip::tcp::socket& tcpSocket() = 0;
};

// Plain (unencrypted) TCP socket implementation of ISocket.
class PlainSocket : public ISocket {
   public:
    explicit PlainSocket(asio::io_context& ioc) : socket_(ioc) {}
    explicit PlainSocket(asio::ip::tcp::socket socket) : socket_(std::move(socket)) {}

    void asyncReadSome(const asio::mutable_buffer& buffer, IoHandler handler) override {
        socket_.async_read_some(buffer, std::move(handler));
    }

    void asyncWrite(const std::vector<asio::const_buffer>& buffers, IoHandler handler) override {
        asio::async_write(socket_, buffers, std::move(handler));
    }

    void close() override {
        std::error_code ec;
        socket_.close(ec);
    }

    void shutdown(std::error_code& ec) override {
        socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    }

    bool isOpen() const override {
        return socket_.is_open();
    }

    bool needsHandshake() const override {
        return false;
    }

    void asyncConnect(const asio::ip::tcp::resolver::results_type& endpoints,
                      ConnectHandler handler) override {
        asio::async_connect(socket_, endpoints, std::move(handler));
    }

    void asyncHandshake(bool /*isServer*/, ErrorHandler handler) override {
        // No TLS — call handler synchronously with success.
        handler(std::error_code());
    }

    asio::ip::tcp::socket& tcpSocket() override {
        return socket_;
    }

   private:
    asio::ip::tcp::socket socket_;
};

}  // namespace beauty
