// Example: a minimal HTTP client built on the Beauty library.
//
// It performs a single HTTP request against the given http:// URL and prints
// the response status, headers and body. By default it issues a GET; pass a
// method and (optionally) a request body to send something else.
//
// Usage:
//   beauty_http_client_example http://127.0.0.1:8080/index.html
//   beauty_http_client_example http://127.0.0.1:8080/api POST '{"k":"v"}'

#include <iostream>
#include <string>

#include <asio.hpp>

#include <beauty/header.hpp>
#include <beauty/http_client.hpp>
#include <beauty/i_http_client_handler.hpp>
#include <beauty/response.hpp>

using namespace beauty;

// Handler that prints the response (or error) and then stops the io_context.
class PrintingHandler : public IHttpClientHandler {
   public:
    explicit PrintingHandler(asio::io_context &ioc) : ioc_(ioc) {}

    void onResponse(const Response &response) override {
        std::cout << "HTTP/" << response.httpVersionMajor_ << "." << response.httpVersionMinor_
                  << " " << response.statusCode_ << " " << response.statusMessage_ << "\n";
        for (const auto &h : response.headers_) {
            std::cout << h.name_ << ": " << h.value_ << "\n";
        }
        std::cout << "\n";
        std::cout.write(response.body_.data(), static_cast<std::streamsize>(response.body_.size()));
        std::cout << "\n";
        ioc_.stop();
    }

    void onError(const std::string &error) override {
        std::cerr << "[error] " << error << "\n";
        ioc_.stop();
    }

   private:
    asio::io_context &ioc_;
};

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " http://host:port/path [METHOD] [body]\n";
        return 1;
    }

    std::string url = argv[1];
    std::string method = (argc >= 3) ? argv[2] : "GET";
    std::string body = (argc >= 4) ? argv[3] : std::string();

    try {
        asio::io_context ioc;
        PrintingHandler handler(ioc);

        HttpClient::Config config;
        config.maxResponseSize = 64 * 1024;
        config.requestTimeout = std::chrono::seconds(10);

        auto client = HttpClient::create(ioc, handler, config);

        std::cout << method << " " << url << " ...\n";

        // HttpClient::request() sets Host and Connection headers
        // automatically. For POST/PUT with a body, use the post()/put()
        // convenience methods which also set Content-Type.
        if (method == "POST" || method == "PUT") {
            client->request(method, url, {{"Content-Type", "application/octet-stream"}}, body);
        } else {
            client->request(method, url);
        }

        ioc.run();
    } catch (std::exception &e) {
        std::cerr << "exception: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
