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

    void sendRequestWithExpect100(const std::string& headers, const std::string& body) {
        // Store the body for later use if we get 100 Continue
        expect100Body_ = body;
        expect100WaitingForFinalResponse_ = false;

        // Send headers first
        std::ostream requestStream(&request_);
        requestStream << headers;
        asio::async_write(
            socket_,
            request_,
            std::bind(&TestClient::handleWriteRequestExpect100, this, asio::placeholders::error));
    }

    void sendMultiPartRequestWithExpect100(const std::string& headers,
                                           const std::deque<std::string>& bodyParts) {
        isMultiPart_ = true;
        // Store the body parts for later use if we get 100 Continue
        multiPartRequests_ = bodyParts;

        // Send headers first
        std::ostream requestStream(&request_);
        requestStream << headers;
        asio::async_write(
            socket_,
            request_,
            std::bind(&TestClient::handleWriteRequestExpect100, this, asio::placeholders::error));
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
        std::string httpVersion_;
    };

    TestResult getConnectedResult() {
        // expectedContentLength_ = expectedContentLength;
        std::unique_lock<std::mutex> lock(mutex_);
        while (true) {
            auto test = gotResult_.wait_for(lock, std::chrono::seconds(2));
            if (test == std::cv_status::timeout) {
                TestResult ret;
                ret.action_ = TestResult::TimedOut;
                ret.statusCode_ = 0;
                ret.headers_.clear();
                ret.content_.clear();
                return ret;
            }
            if (testResult_.action_ == TestResult::Opened ||
                testResult_.action_ == TestResult::TimedOut ||
                testResult_.action_ == TestResult::Failed) {
                return testResult_;
            }
        }
        return testResult_;
    }

    TestResult getStatusLineResult() {
        std::unique_lock<std::mutex> lock(mutex_);

        // Check if we already have the result
        if (testResult_.action_ == TestResult::ReadRequestStatus ||
            testResult_.action_ == TestResult::TimedOut ||
            testResult_.action_ == TestResult::Failed) {
            return testResult_;
        }

        while (true) {
            auto test = gotResult_.wait_for(lock, std::chrono::seconds(2));
            if (test == std::cv_status::timeout) {
                TestResult ret;
                ret.action_ = TestResult::TimedOut;
                return ret;
            }
            if (testResult_.action_ == TestResult::ReadRequestStatus ||
                testResult_.action_ == TestResult::TimedOut ||
                testResult_.action_ == TestResult::Failed) {
                return testResult_;
            }
        }
    }

    TestResult getHeaderResult() {
        std::unique_lock<std::mutex> lock(mutex_);

        // Check if we already have the result (notification might have happened before we started
        // waiting)
        if (testResult_.action_ == TestResult::ReadHeaders ||
            testResult_.action_ == TestResult::TimedOut ||
            testResult_.action_ == TestResult::Failed) {
            return testResult_;
        }

        while (true) {
            auto test = gotResult_.wait_for(lock, std::chrono::seconds(2));
            if (test == std::cv_status::timeout) {
                TestResult ret;
                ret.action_ = TestResult::TimedOut;
                return ret;
            }
            if (testResult_.action_ == TestResult::ReadHeaders ||
                testResult_.action_ == TestResult::TimedOut ||
                testResult_.action_ == TestResult::Failed) {
                return testResult_;
            }
        }
    }

    TestResult getContentResult(size_t expectedContentLength) {
        expectedContentLength_ = expectedContentLength;
        std::unique_lock<std::mutex> lock(mutex_);

        // Check if we already have the result (notification might have happened before we started
        // waiting)
        if (testResult_.action_ == TestResult::ReadContent ||
            testResult_.action_ == TestResult::TimedOut ||
            testResult_.action_ == TestResult::Failed) {
            return testResult_;
        }

        while (true) {
            auto test = gotResult_.wait_for(lock, std::chrono::seconds(2));
            if (test == std::cv_status::timeout) {
                std::cout << "Timeout in getContentResult, final action: " << testResult_.action_
                          << std::endl;
                TestResult ret;
                ret.action_ = TestResult::TimedOut;
                return ret;
            }
            if (testResult_.action_ == TestResult::ReadContent ||
                testResult_.action_ == TestResult::TimedOut ||
                testResult_.action_ == TestResult::Failed) {
                return testResult_;
            }
        }
    }

    TestResult getExpect100Result() {
        std::unique_lock<std::mutex> lock(mutex_);

        while (true) {
            auto test = gotResult_.wait_for(
                lock, std::chrono::seconds(5));  // Longer timeout for 100-continue
            if (test == std::cv_status::timeout) {
                std::cout << "Timeout in getExpect100Result, final action: " << testResult_.action_
                          << std::endl;
                TestResult ret;
                ret.action_ = TestResult::TimedOut;
                return ret;
            }
            if (testResult_.action_ == TestResult::ReadContent ||
                testResult_.action_ == TestResult::ReadHeaders ||
                testResult_.action_ == TestResult::TimedOut ||
                testResult_.action_ == TestResult::Failed) {
                return testResult_;
            }
        }
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
            testResult_.httpVersion_ = httpVersion;
            testResult_.statusCode_ = statusCode;
            // Trim whitespace and control characters for clean debug output
            std::string cleanStatusMessage = statusMessage;
            cleanStatusMessage.erase(0, cleanStatusMessage.find_first_not_of(" \t\r\n"));
            cleanStatusMessage.erase(cleanStatusMessage.find_last_not_of(" \t\r\n") + 1);
            // printf("Status line: %s %u %s\n", httpVersion.c_str(), statusCode,
            // cleanStatusMessage.c_str());

            if (!responseStream) {
                std::cout << "Invalid response stream after parsing\n";
                std::cout << "httpVersion: '" << httpVersion << "'\n";
                std::cout << "statusCode: " << statusCode << "\n";
                std::cout << "statusMessage: '" << statusMessage << "'\n";
                std::cout << "Stream state - fail: " << responseStream.fail()
                          << ", bad: " << responseStream.bad() << ", eof: " << responseStream.eof()
                          << "\n";
                return;
            }
            if (httpVersion.substr(0, 5) != "HTTP/") {
                std::cout << "Invalid protocol\n";
                return;
            }

            // Read the response headers, which are terminated by a blank line.
            asio::async_read_until(
                socket_,
                response_,
                "\r\n\r\n",
                std::bind(&TestClient::handleReadHeaders, this, asio::placeholders::error));
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
                // Remove trailing '\r' if present
                if (!header.empty() && header.back() == '\r') {
                    header.pop_back();
                }
                testResult_.headers_.push_back(header);
            }

            // Check headers and behave like a normal HTTP client.
            bool connectionClose = false;
            size_t contentLength = 0;
            for (const auto& h : testResult_.headers_) {
                if (h.find("Connection: close") != std::string::npos) {
                    connectionClose = true;
                }
                if (h.find("Content-Length:") != std::string::npos) {
                    auto pos = h.find(":");
                    if (pos != std::string::npos) {
                        auto lenStr = h.substr(pos + 1);
                        lenStr.erase(0, lenStr.find_first_not_of(" \t"));
                        try {
                            contentLength = std::stoul(lenStr);
                        } catch (const std::exception& e) {
                            std::cout << "Failed to parse Content-Length: " << e.what()
                                      << std::endl;
                        }
                    }
                }
            }

            if (expectedContentLength_ == 0) {
                expectedContentLength_ = contentLength;
            }

            // Check if we can complete the request immediately
            bool canCompleteImmediately = false;
            if (contentLength > 0) {
                if (response_.size() >= contentLength) {
                    // Content already available
                    canCompleteImmediately = true;
                } else {
                    // Need to read more content
                    canCompleteImmediately = false;
                }
            } else {
                // No body expected
                canCompleteImmediately = true;
            }

            if (canCompleteImmediately) {
                // Skip ReadHeaders notification and go directly to ReadContent
                if (contentLength > 0) {
                    // Content already in buffer, process immediately
                    testResult_.content_.resize(contentLength);
                    asio::buffer_copy(asio::buffer(testResult_.content_), response_.data());
                    testResult_.action_ = TestResult::ReadContent;
                    gotResult_.notify_one();
                } else {
                    // No body expected, setting ReadHeaders
                    testResult_.action_ = TestResult::ReadHeaders;
                    gotResult_.notify_one();
                }
            } else {
                // Normal flow - notify for headers first, then set up content reading
                testResult_.action_ = TestResult::ReadHeaders;
                gotResult_.notify_one();

                if (connectionClose) {
                    // The server will close the connection after sending the response.
                    // We can read until EOF.
                    asio::async_read(
                        socket_,
                        response_,
                        asio::transfer_at_least(1),
                        std::bind(&TestClient::handleReadContent, this, asio::placeholders::error));
                } else {
                    // Start reading the remaining body bytes
                    std::cout << "Need to read more content\n";
                    asio::async_read(
                        socket_,
                        response_,
                        asio::transfer_exactly(contentLength - response_.size()),
                        std::bind(&TestClient::handleReadContent, this, asio::placeholders::error));
                }
            }
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

    // 100-continue specific handlers
    void handleWriteRequestExpect100(const std::error_code& err) {
        if (!err) {
            // Clear response buffer for fresh read
            response_.consume(response_.size());

            // Read the response status line (could be 100 Continue or final response)
            asio::async_read_until(
                socket_,
                response_,
                "\r\n",
                std::bind(
                    &TestClient::handleReadStatusLineExpect100, this, asio::placeholders::error));
        } else {
            std::cout << "Error Expect100 Write: " << err.message() << ":" << err.value() << "\n";
        }
    }

    void handleReadStatusLineExpect100(const std::error_code& err) {
        if (!err) {
            std::istream responseStream(&response_);
            std::string httpVersion;
            responseStream >> httpVersion;
            unsigned int statusCode;
            responseStream >> statusCode;
            std::string statusMessage;
            std::getline(responseStream, statusMessage);

            if (statusCode == 100) {
                // Got 100 Continue, read the remaining \r\n to complete the response
                asio::async_read_until(socket_,
                                       response_,
                                       "\r\n",
                                       std::bind(&TestClient::handleRead100ContinueHeaders,
                                                 this,
                                                 asio::placeholders::error));
            } else {
                // Got final response, process as normal
                testResult_.httpVersion_ = httpVersion;
                testResult_.statusCode_ = statusCode;
                expect100WaitingForFinalResponse_ = false;

                // Read headers for final response
                asio::async_read_until(
                    socket_,
                    response_,
                    "\r\n\r\n",
                    std::bind(
                        &TestClient::handleReadHeadersExpect100, this, asio::placeholders::error));
            }
        } else {
            std::cout << "Error Expect100 Status: " << err.message() << ":" << err.value() << "\n";
        }
    }

    void handleRead100ContinueHeaders(const std::error_code& err) {
        if (!err) {
            // For 100 Continue, we just need to consume the remaining \r\n
            // The 100 Continue response is: "HTTP/1.1 100 Continue\r\n\r\n"
            // We already read the status line, so we just need to consume the final \r\n
            std::istream responseStream(&response_);
            std::string emptyLine;
            std::getline(responseStream, emptyLine);  // Should be just \r

            // Now send the body (either single body or multipart)
            if (isMultiPart_ && !multiPartRequests_.empty()) {
                // Send first multipart body
                auto firstPart = multiPartRequests_.front();
                multiPartRequests_.pop_front();
                asio::async_write(socket_,
                                  asio::buffer(firstPart),
                                  std::bind(&TestClient::handleWriteMultiPartBodyExpect100,
                                            this,
                                            asio::placeholders::error));
            } else {
                // Send single body
                asio::async_write(
                    socket_,
                    asio::buffer(expect100Body_),
                    std::bind(
                        &TestClient::handleWriteBodyExpect100, this, asio::placeholders::error));
            }
        } else {
            std::cout << "Error Expect100 Continue Headers: " << err.message() << ":" << err.value()
                      << "\n";
        }
    }

    void handleWriteBodyExpect100(const std::error_code& err) {
        if (!err) {
            expect100WaitingForFinalResponse_ = true;

            // Now read the final response status line
            asio::async_read_until(socket_,
                                   response_,
                                   "\r\n",
                                   std::bind(&TestClient::handleReadFinalStatusLineExpect100,
                                             this,
                                             asio::placeholders::error));
        } else {
            std::cout << "Error Expect100 Body Write: " << err.message() << ":" << err.value()
                      << "\n";
        }
    }

    void handleWriteMultiPartBodyExpect100(const std::error_code& err) {
        if (!err) {
            if (!multiPartRequests_.empty()) {
                // Send next multipart piece
                auto nextPart = multiPartRequests_.front();
                multiPartRequests_.pop_front();
                asio::async_write(socket_,
                                  asio::buffer(nextPart),
                                  std::bind(&TestClient::handleWriteMultiPartBodyExpect100,
                                            this,
                                            asio::placeholders::error));
            } else {
                // All multipart pieces sent, now read the final response
                expect100WaitingForFinalResponse_ = true;
                isMultiPart_ = false;  // Reset multipart flag

                // Now read the final response status line
                asio::async_read_until(socket_,
                                       response_,
                                       "\r\n",
                                       std::bind(&TestClient::handleReadFinalStatusLineExpect100,
                                                 this,
                                                 asio::placeholders::error));
            }
        } else {
            std::cout << "Error Expect100 MultiPart Body Write: " << err.message() << ":"
                      << err.value() << "\n";
        }
    }

    void handleReadFinalStatusLineExpect100(const std::error_code& err) {
        if (!err) {
            std::istream responseStream(&response_);
            std::string httpVersion;
            responseStream >> httpVersion;
            unsigned int statusCode;
            responseStream >> statusCode;
            std::string statusMessage;
            std::getline(responseStream, statusMessage);

            testResult_.httpVersion_ = httpVersion;
            testResult_.statusCode_ = statusCode;

            // Read final response headers
            asio::async_read_until(
                socket_,
                response_,
                "\r\n\r\n",
                std::bind(
                    &TestClient::handleReadHeadersExpect100, this, asio::placeholders::error));
        } else {
            std::cout << "Error Expect100 Final Status: " << err.message() << ":" << err.value()
                      << "\n";
        }
    }

    void handleReadHeadersExpect100(const std::error_code& err) {
        if (!err) {
            // Process the response headers
            std::istream responseStream(&response_);
            std::string header;
            while (std::getline(responseStream, header) && header != "\r") {
                if (!header.empty() && header.back() == '\r') {
                    header.pop_back();
                }
                testResult_.headers_.push_back(header);
            }

            // Check for Content-Length
            size_t contentLength = 0;
            for (const auto& h : testResult_.headers_) {
                if (h.find("Content-Length:") != std::string::npos) {
                    auto pos = h.find(":");
                    if (pos != std::string::npos) {
                        auto lenStr = h.substr(pos + 1);
                        lenStr.erase(0, lenStr.find_first_not_of(" \t"));
                        try {
                            contentLength = std::stoul(lenStr);
                        } catch (const std::exception& e) {
                            std::cout << "Failed to parse Content-Length: " << e.what()
                                      << std::endl;
                        }
                    }
                }
            }

            if (contentLength > 0) {
                // Read content
                if (response_.size() >= contentLength) {
                    // Content already available
                    testResult_.content_.resize(contentLength);
                    asio::buffer_copy(asio::buffer(testResult_.content_), response_.data());
                    testResult_.action_ = TestResult::ReadContent;
                    gotResult_.notify_one();
                } else {
                    // Need to read more content
                    asio::async_read(socket_,
                                     response_,
                                     asio::transfer_exactly(contentLength - response_.size()),
                                     std::bind(&TestClient::handleReadContentExpect100,
                                               this,
                                               asio::placeholders::error));
                }
            } else {
                // No content expected
                testResult_.action_ = TestResult::ReadHeaders;
                gotResult_.notify_one();
            }
        } else {
            std::cout << "Error Expect100 Headers: " << err.message() << ":" << err.value() << "\n";
        }
    }

    void handleReadContentExpect100(const std::error_code& err) {
        if (!err) {
            // Content is now available in response_ buffer
            testResult_.content_.resize(response_.size());
            asio::buffer_copy(asio::buffer(testResult_.content_), response_.data());
            testResult_.action_ = TestResult::ReadContent;
            gotResult_.notify_one();
        } else {
            std::cout << "Error Expect100 Content: " << err.message() << ":" << err.value() << "\n";
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

    // 100-continue support
    std::string expect100Body_;
    bool expect100WaitingForFinalResponse_ = false;
};
