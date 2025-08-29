#include <iostream>
#include <string>
#include <asio.hpp>
#include <beauty/server.hpp>

#include "file_io.hpp"
#include "my_file_api.hpp"
#include "my_router_api.hpp"

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
        Server s(ioc, argv[1], argv[2], &fileIO, persistentOption, 1024);

        MyRouterApi routerApi;
        MyFileApi fileApi(argv[3]);
        s.addRequestHandler(std::bind(&MyRouterApi::handleRequest, &routerApi, _1, _2));
        // Must be placed last, after other middleware, as it (in this example)
        // prepares filePath_ and and reply headers for FileIO.
        s.addRequestHandler(std::bind(&MyFileApi::handleRequest, &fileApi, _1, _2));
        s.setDebugMsgHandler([](const std::string &msg) { std::cout << msg << std::endl; });

        // Run the server until stopped with Ctrl-C.
        ioc.run();
    } catch (std::exception &e) {
        std::cerr << "exception: " << e.what() << "\n";
    }

    return 0;
}
