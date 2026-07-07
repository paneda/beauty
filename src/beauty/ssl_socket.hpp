#pragma once
#include "beauty/i_socket.hpp"

#include <asio/ssl.hpp>

namespace beauty {

// SSL/TLS socket implementation of ISocket. Wraps an asio::ssl::stream over a
// TCP socket. Only available when the build has SSL support enabled.
class SslSocket : public ISocket {
   public:
    SslSocket(asio::io_context& ioc, asio::ssl::context& sslCtx) : stream_(ioc, sslCtx) {}

    void asyncReadSome(const asio::mutable_buffer& buffer, IoHandler handler) override {
        stream_.async_read_some(buffer, std::move(handler));
    }

    void asyncWrite(const std::vector<asio::const_buffer>& buffers, IoHandler handler) override {
        asio::async_write(stream_, buffers, std::move(handler));
    }

    void close() override {
        std::error_code ec;
        stream_.next_layer().close(ec);
    }

    void shutdown(std::error_code& ec) override {
        stream_.next_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ec);
    }

    bool isOpen() const override { return stream_.next_layer().is_open(); }

    bool needsHandshake() const override { return true; }

    void asyncConnect(const asio::ip::tcp::resolver::results_type& endpoints,
                      ConnectHandler handler) override {
        asio::async_connect(stream_.next_layer(), endpoints, std::move(handler));
    }

    void asyncHandshake(bool isServer, ErrorHandler handler) override {
        stream_.async_handshake(isServer ? asio::ssl::stream_base::server
                                         : asio::ssl::stream_base::client,
                                std::move(handler));
    }

    asio::ip::tcp::socket& tcpSocket() override { return stream_.next_layer(); }

   private:
    asio::ssl::stream<asio::ip::tcp::socket> stream_;
};

}  // namespace beauty
