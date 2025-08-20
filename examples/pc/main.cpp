#include <iostream>
#include <string>
#include <asio.hpp>
#include <beauty/server.hpp>

#include "file_io.hpp"
#include "my_file_api.hpp"

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
        // Initialise the server.
        FileIO fileIO(argv[3]);
        HttpPersistence persistentOption(5s, 1000, 0);
        MyFileApi fileApi(argv[3]);
        Server s(ioc, argv[1], argv[2], &fileIO, persistentOption, 1024);
        s.addRequestHandler(std::bind(&MyFileApi::handleRequest, &fileApi, _1, _2));
        s.setDebugMsgHandler([](const std::string &msg) { std::cout << msg << std::endl; });

        // Run the server until stopped with Ctrl-C.
        ioc.run();
    } catch (std::exception &e) {
        std::cerr << "exception: " << e.what() << "\n";
    }

    return 0;
}
