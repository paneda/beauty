#include <asio.hpp>
#include <condition_variable>
#include <deque>
#include <functional>
#include <iostream>
#include <istream>
#include <mutex>
#include <ostream>
#include <string>

class TestClient {
   public:
    TestClient(asio::io_context& ioc) : resolver_(ioc), socket_(ioc) {}
    TestClient(const TestClient&) = delete;
    TestClient& operator=(const TestClient&) = delete;
    void connect(const std::string& server, const std::string& port) {
        // Start an asynchronous resolve to translate the server and service names
        // into a list of endpoints.
        resolver_.async_resolve(server,
                                port,
                                std::bind(&TestClient::handleResolve,
                                          this,
                                          asio::placeholders::error,
                                          asio::placeholders::results));
    }

    void sendRequest(const std::string& request) {
        std::ostream requestStream(&request_);
        requestStream << request;
        asio::async_write(
            socket_,
            request_,
            std::bind(&TestClient::handleWriteRequest, this, asio::placeholders::error));
    }

    void sendMultiPartRequest(const std::deque<std::string>& requests) {
        isMultiPart_ = true;
        multiPartRequests_ = requests;
        auto request = multiPartRequests_.front();
        multiPartRequests_.pop_front();
        std::ostream requestStream(&request_);
        requestStream << request;
        asio::async_write(
            socket_,
            request_,
            std::bind(&TestClient::handleWriteRequest, this, asio::placeholders::error));
    }

    struct TestResult {
        enum Action {
            None,
            Failed,
            Opened,
            Closed,
            ReadRequestStatus,
            ReadHeaders,
            ReadContent,
            TimedOut
        } action_;
        std::deque<std::string> headers_;
        std::vector<char> content_;
        int statusCode_;
    };

    TestResult getResult(size_t expectedContentLength) {
        expectedContentLength_ = expectedContentLength;
        std::unique_lock<std::mutex> lock(mutex_);
        auto test = gotResult_.wait_for(lock, std::chrono::seconds(2));
        if (test == std::cv_status::timeout) {
            TestResult ret;
            ret.action_ = TestResult::TimedOut;
            ret.statusCode_ = 0;
            ret.headers_.clear();
            ret.content_.clear();
            return ret;
        }
        return testResult_;
    }

   private:
    void handleResolve(const std::error_code& err,
                       const asio::ip::tcp::resolver::results_type& endpoints) {
        if (!err) {
            // Attempt a connection to each endpoint in the list until we
            // successfully establish a connection.
            asio::async_connect(
                socket_,
                endpoints,
                std::bind(&TestClient::handleConnect, this, asio::placeholders::error));
        } else {
            std::cout << "Error1: " << err.message() << ":" << err.value() << "\n";
        }
    }

    void handleConnect(const std::error_code& err) {
        if (!err) {
            testResult_.action_ = TestResult::Opened;
            gotResult_.notify_one();
        } else {
            std::cout << "Error2: " << err.message() << ":" << err.value() << "\n";
        }
    }

    void handleWriteRequest(const std::error_code& err) {
        if (!err) {
            // Read the response status line. The response_ streambuf will
            // automatically grow to accommodate the entire line. The growth may be
            // limited by passing a maximum size to the streambuf constructor.
            asio::async_read_until(
                socket_,
                response_,
                "\r\n",
                std::bind(&TestClient::handleReadStatusLine, this, asio::placeholders::error));
        } else {
            std::cout << "Error3: " << err.message() << ":" << err.value() << "\n";
        }
    }

    void handleReadStatusLine(const std::error_code& err) {
        if (!err) {
            // Check that response is OK.
            std::istream responseStream(&response_);
            std::string httpVersion;
            responseStream >> httpVersion;
            unsigned int statusCode;
            responseStream >> statusCode;
            std::string statusMessage;
            std::getline(responseStream, statusMessage);

            testResult_.action_ = TestResult::ReadRequestStatus;
            gotResult_.notify_one();
            testResult_.statusCode_ = statusCode;

            if (!responseStream) {
                std::cout << "Invalid response stream\n";
                return;
            }
            if (httpVersion.substr(0, 5) != "HTTP/") {
                std::cout << "Invalid protocol\n";
                return;
            }
            if (statusCode != 200 && statusCode != 201) {
                return;
            }
            if (!isMultiPart_) {
                // Read the response headers, which are terminated by a blank line.
                asio::async_read_until(
                    socket_,
                    response_,
                    "\r\n\r\n",
                    std::bind(&TestClient::handleReadHeaders, this, asio::placeholders::error));
            } else {
                if (!multiPartRequests_.empty()) {
                    response_.consume(response_.size());
                    auto request = multiPartRequests_.front();
                    multiPartRequests_.pop_front();
                    std::ostream requestStream(&request_);
                    requestStream << request;
                    asio::async_write(
                        socket_,
                        request_,
                        std::bind(
                            &TestClient::handleWriteRequest, this, asio::placeholders::error));
                    return;
                }
                return;
            }
        } else {
            std::cout << "Error4: " << err.message() << ":" << err.value() << "\n";
        }
    }

    void handleReadHeaders(const std::error_code& err) {
        if (!err) {
            // Process the response headers.
            std::istream responseStream(&response_);
            std::string header;
            while (std::getline(responseStream, header) && header != "\r") {
                testResult_.headers_.push_back(header);
            }
            testResult_.action_ = TestResult::ReadHeaders;
            gotResult_.notify_one();

            // Write whatever content we already have to output.
            if (response_.size() > 0) {
                if (response_.size() == expectedContentLength_) {
                    testResult_.content_.resize(response_.size() /
                                                sizeof(decltype(testResult_.content_)::value_type));
                    asio::buffer_copy(asio::buffer(testResult_.content_), response_.data());
                    testResult_.action_ = TestResult::ReadContent;
                    gotResult_.notify_one();
                }
            }

            // Start reading remaining data until EOF.
            asio::async_read(
                socket_,
                response_,
                asio::transfer_at_least(1),
                std::bind(&TestClient::handleReadContent, this, asio::placeholders::error));
        } else {
            std::cout << "Error5: " << err.message() << ":" << err.value() << "\n";
        }
    }

    void handleReadContent(const std::error_code& err) {
        if (!err) {
            if (response_.size() == expectedContentLength_) {
                testResult_.content_.resize(response_.size() /
                                            sizeof(decltype(testResult_.content_)::value_type));
                asio::buffer_copy(asio::buffer(testResult_.content_), response_.data());
                testResult_.action_ = TestResult::ReadContent;
                gotResult_.notify_one();
            }

            // Continue reading remaining data until EOF.
            asio::async_read(
                socket_,
                response_,
                asio::transfer_at_least(1),
                std::bind(&TestClient::handleReadContent, this, asio::placeholders::error));
        } else if (err != asio::error::eof) {
            std::cout << "Error6: " << err.message() << ":" << err.value() << "\n";
        }
    }

    asio::ip::tcp::resolver resolver_;
    asio::ip::tcp::socket socket_;
    asio::streambuf request_;
    asio::streambuf response_;
    size_t expectedContentLength_;
    std::mutex mutex_;
    std::condition_variable gotResult_;
    TestResult testResult_;
    std::deque<std::string> multiPartRequests_;
    bool isMultiPart_ = false;
};
