#include <asio.hpp>
#include <iostream>
#include <string>

#include "file_handler.hpp"
#include "server.hpp"

int main(int argc, char *argv[]) {
    try {
        // Check command line arguments.
        if (argc != 4) {
            std::cerr << "Usage: http_server <address> <port> <doc_root>\n";
            std::cerr << "  For IPv4, try:\n";
            std::cerr << "    receiver 0.0.0.0 80 .\n";
            std::cerr << "  For IPv6, try:\n";
            std::cerr << "    receiver 0::0 80 .\n";
            return 1;
        }

        asio::io_context ioc;
        // Initialise the server.
        http::server::FileHandler fileHandler(argv[3]);
        http::server::Server s(ioc, argv[1], argv[2], &fileHandler);

        // Run the server until stopped.
        ioc.run();
    } catch (std::exception &e) {
        std::cerr << "exception: " << e.what() << "\n";
    }

    return 0;
}
