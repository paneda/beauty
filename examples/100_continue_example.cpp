#include "beauty/server.hpp"
#include "beauty/file_io_mock.hpp"
#include <iostream>

/*
 * Example showing how to use the new 100-continue functionality
 *
 * This demonstrates how a server can validate headers before accepting
 * a potentially large request body, which is useful for:
 * - Authentication checks
 * - Content-Type validation
 * - Content-Length limits
 * - Authorization checks
 */

int main() {
    asio::io_context ioContext;

    // Create a mock file I/O for this example
    beauty::FileIOMock fileIO;

    // Create server with keep-alive settings
    beauty::HttpPersistence options(std::chrono::seconds(30), 100, 1000);
    beauty::Server server(ioContext, "127.0.0.1", "8080", &fileIO, options);

    // Set up an Expect: 100-continue handler
    server.setExpectContinueHandler([](unsigned connectionId, const beauty::Request& req) -> bool {
        std::cout << "100-continue requested for connection " << connectionId << std::endl;
        std::cout << "Method: " << req.method_ << ", URI: " << req.uri_ << std::endl;

        // Example validation logic:

        // 1. Check authentication
        std::string authHeader = req.getHeaderValue("Authorization");
        if (authHeader.empty()) {
            std::cout << "Rejecting: Missing Authorization header" << std::endl;
            return false;  // Reject - will send 400 Bad Request
        }

        // 2. Check content type for POST/PUT requests
        if (req.method_ == "POST" || req.method_ == "PUT") {
            std::string contentType = req.getHeaderValue("Content-Type");
            if (contentType.find("application/json") == std::string::npos &&
                contentType.find("multipart/form-data") == std::string::npos) {
                std::cout << "Rejecting: Unsupported Content-Type: " << contentType << std::endl;
                return false;
            }
        }

        // 3. Check content length limits
        std::string contentLengthStr = req.getHeaderValue("Content-Length");
        if (!contentLengthStr.empty()) {
            size_t contentLength = std::stoul(contentLengthStr);
            const size_t MAX_CONTENT_LENGTH = 10 * 1024 * 1024;  // 10MB limit
            if (contentLength > MAX_CONTENT_LENGTH) {
                std::cout << "Rejecting: Content too large: " << contentLength << " bytes"
                          << std::endl;
                return false;
            }
        }

        std::cout << "Approving request - sending 100 Continue" << std::endl;
        return true;  // Approve - will send 100 Continue
    });

    // Set up normal request handlers that will be called after body is received
    server.addRequestHandler([](const beauty::Request& req, beauty::Reply& reply) {
        if (req.expectsContinue()) {
            std::cout << "Processing request that used 100-continue" << std::endl;
            std::cout << "Body size: " << req.body_.size() << " bytes" << std::endl;
        }

        if (req.method_ == "POST" && req.requestPath_ == "/upload") {
            reply.status_ = beauty::Reply::created;
            reply.content_ = "Upload successful!";
            reply.addHeader("Content-Type", "text/plain");
        } else {
            reply.status_ = beauty::Reply::ok;
            reply.content_ = "Hello from Beauty HTTP server with 100-continue support!";
            reply.addHeader("Content-Type", "text/plain");
        }
    });

    std::cout << "Server listening on http://127.0.0.1:8080" << std::endl;
    std::cout << "Try sending requests with 'Expect: 100-continue' header" << std::endl;
    std::cout << "Example curl command:" << std::endl;
    std::cout << "curl -H \"Expect: 100-continue\" -H \"Authorization: Bearer token123\" \\"
              << std::endl;
    std::cout << "     -H \"Content-Type: application/json\" \\" << std::endl;
    std::cout << "     -d '{\"test\": \"data\"}' http://127.0.0.1:8080/upload" << std::endl;

    ioContext.run();

    return 0;
}
