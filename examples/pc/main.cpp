#include <iostream>
#include <string>
#include <thread>
#include <csignal>
#include <asio.hpp>
#include <beauty/server.hpp>

#include "file_io.hpp"
#include "my_file_api.hpp"
#include "my_router_api.hpp"
#include "my_data_streaming_endpoint.hpp"
#include "my_chat_endpoint.hpp"

using namespace std::literals::chrono_literals;
using namespace beauty;
using namespace std::placeholders;

int main(int argc, char *argv[]) {
    try {
        // Check command line arguments.
        if (argc != 4) {
            std::cerr << "Usage: beauty_example <address> <port> <doc_root>\n";
            std::cerr << "  For IPv4, try:\n";
            std::cerr << "    beauty_example 0.0.0.0 80 .\n";
            std::cerr << "  For IPv6, try:\n";
            std::cerr << "    beauty_example 0::0 80 .\n";
            return 1;
        }

        asio::io_context ioc;
        // Create the server.
        Settings Settings(5s, 1000, 0);
        Server s(ioc, argv[1], argv[2], Settings, 1024);

        // The rest of the setup, done below, are all optional depending on
        // your use case.

        // Set up file I/O for static file serving
        FileIO fileIO(argv[3]);
        s.setFileIO(&fileIO);

        // Set up a custom Expect: 100-continue handler for authentication,
        // useful for large uploads where you want to reject requests before
        // reading the body.
        s.setExpectContinueHandler([](const Request &req, Reply &rep) -> void {
            std::cout << "Expect: 100-continue received for " << req.requestPath_ << std::endl;
            auto authHeader = req.getHeaderValue("authorization");
            if (authHeader.size() < 8 || authHeader.substr(0, 6) != "Bearer") {
                rep.addHeader("WWW-Authenticate", "Basic realm=\"Access to the site\"");
                rep.send(Reply::status_type::unauthorized);
                return;
            }
            auto token = req.getHeaderValue("authorization").substr(7);
            if (token != "valid_token") {
                rep.addHeader("WWW-Authenticate", "Basic realm=\"Access to the site\"");
                rep.send(Reply::status_type::unauthorized);
                return;
            }
            printf("Authorized with token: %s\n", token.c_str());
            rep.send(Reply::status_type::ok);  // Approve the request (will return 100 Continue)
        });

        // Set up HTTP request handlers, these provide REST API:s for our
        // Server. See examples/pc/my_router_api.hpp and
        // examples/pc/my_file_api.hpp for implementation.
        MyRouterApi routerApi;
        MyFileApi fileApi(argv[3]);
        s.addRequestHandler(std::bind(&MyRouterApi::handleRequest, &routerApi, _1, _2));
        s.addRequestHandler(std::bind(&MyFileApi::handleRequest, &fileApi, _1, _2));

        // Set up WebSocket endpoints
        auto chatEndpoint = std::make_shared<MyChatEndpoint>();
        auto dataEndpoint = std::make_shared<MyDataStreamingEndpoint>();

        s.setWsEndpoints({chatEndpoint, dataEndpoint});

        // Set up debug message handler, useful for troubleshooting.
        // s.setDebugMsgHandler([](const std::string &msg) { std::cout << "[DEBUG] " << msg <<
        // std::endl; });

        // Set up signal handling for graceful shutdown
        asio::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait([&](std::error_code /*ec*/, int /*signo*/) {
            std::cout << "\nShutting down server...\n";
            ioc.stop();
        });

        // Set up Asio timer for periodic data broadcasting on dataEndpoint
        asio::steady_timer dataTimer(ioc);
        int broadcastCounter = 0;
        std::function<void()> scheduleBroadcast = [&]() {
            broadcastCounter++;

            // Generate new data every 10 ticks (1 second) but process queues every tick (100ms)
            if (broadcastCounter >= 10) {
                dataEndpoint->broadcastData();
                broadcastCounter = 0;
            } else {
                // Just process queues without generating new data
                dataEndpoint->processQueues();
            }

            dataTimer.expires_after(100ms);
            dataTimer.async_wait([&](std::error_code ec) {
                if (!ec) {
                    scheduleBroadcast();
                }
                // Timer will stop automatically when io_context is stopped
            });
        };
        scheduleBroadcast();

        std::cout << "\n";
        std::cout << "========================================\n";
        std::cout << " Beauty HTTP/WebSocket Demo Started\n";
        std::cout << "========================================\n";
        std::cout << "\n";
        std::cout << "Web Interface:\n";
        std::cout << "  Main Demo UI: http://localhost:" << argv[2] << "/\n";
        std::cout << "Test_scripts: test_scripts/\n";
        std::cout << "  Advanced tests using curl/python (see directory for details)" << argv[2]
                  << "/\n";
        std::cout << "\n";
        std::cout << "HTTP API Endpoints:\n";
        std::cout << "  API without Router:  /list-files, /download-file\n";
        std::cout << "     Simple API demonstration\n";
        std::cout << "  API with Router:  /api/users/*\n";
        std::cout << "     RESTful API with path parameters and CORS support\n";
        std::cout << "  Static Files:     /* (serves from document root)\n";
        std::cout << "\n";
        std::cout << "WebSocket Endpoints:\n";
        std::cout << "  Chat Endpoint: ws://localhost:" << argv[2] << "/ws/chat\n";
        std::cout << "     Interactive messaging with multiple clients\n";
        std::cout << "  Data Stream Endpoint: ws://localhost:" << argv[2] << "/ws/data\n";
        std::cout << "     Flow control demo (bursty data production scenarios)\n";
        std::cout << "\n";
        std::cout << "Features Demonstrated:\n";
        std::cout << "  • Static file serving with ETag and Cache-Control\n";
        std::cout << "  • Multipart file uploads with progress tracking\n";
        std::cout << "  • RESTful API routing with parameter extraction and CORS support\n";
        std::cout << "  • Expect: 100-continue with authentication check\n";
        std::cout
            << "  • WebSocket (simple chat and data streaming (drop-on-busy vs queue-based)\n";
        std::cout << "  • Graceful shutdown with signal handling\n";
        std::cout << "\n";
        std::cout << "Quick Start: Open http://localhost:" << argv[2] << "/ in your browser\n";
        std::cout << "Stop Server: Press Ctrl+C for graceful shutdown\n";
        std::cout << "===============================================\n";

        // Run the server until stopped with Ctrl-C.
        ioc.run();
    } catch (std::exception &e) {
        std::cerr << "exception: " << e.what() << "\n";
    }

    return 0;
}
