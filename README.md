# Beauty
Beauty is a web server designed for constrained environments. In particular it
was developed to run on an ESP32.
However it will run in any environment supporting asio (non-boost).

It is insipired by Express.js in that it executes a stack of middlewares that
are executed in added order. See examples.

Its main properties:
* Supports HTTP 1.1 (configurable support for keep-alive connections)
* Multi-part file upload
* Adaptable to any file system (e.g. LittleFs on ESP32 or std::fstream)
* Fast, asynchronous and lock-free implementation
* Low heap memory requirement (configurable support of buffer sizes)
* No ESP-IDF/Arduino dependencies, i.e. the web application can be mocked and
  and tested on PC during development

# Dependencies
* asio (non-boost)
* c++11

# Usage

## PC (e.g. Linux/Windows)
See examples/pc/main.cpp
This example can be build and run as:
```
mkdir build
cmake --build build ..
# run with
build/examples/beauty_example 127.0.0.1 8080 www
# visit 127.0.0.1:8080 and test the routes provided by examples/pc/my_file_api.cpp
# upload this README.md with:
# curl --location '127.0.0.1:8080' --form 'file1=@"README.md"' 
```

## ESP32
The below code assumes running Beauty in an platform.io/Arduino context However
there's no reason it shouldn't run using ESP-IDF.

As asio::io_context::run() is blocking, the code below uses a RTOS thread as
its probably the best fit for most applications. However if nothing else
needs to run in loop(), the RTOS thread can be omitted and the code below can
be simplified to the "standard" Arduino setup() and loop() concept.

```
// included for this example
#include <functional>
#include <chrono>
#include <Arduino.h>
#include <WiFi.h>
#include <asio.hpp>
#include <server.hpp>
#include <reply.hpp>

// these includes needs implementation, see examples
#include "my_file_io.hpp"
#include "my_file_api.hpp"

// somewhere in main..
void httpServerThread(void*) {
    // Wifi needs to be setup in the same thread
    WiFi.onEvent(WiFiEvent);
    WiFi.begin();

    MyFileIO fio; // see examples folder for LittleFs

    // configurable keep-alive support
    beauty::HttpPersistence persistentOption(std::chrono::seconds(5), 1000, 20);

    // the asio::context
    asio::io_context ioc;

    // must use an alternative constructor compared to PC example
    beauty::Server server(ioc, 80, &fio, persistentOption, 1024);

    // middlewares (just one in this example), see examples folder
    MyFileApi fileApiHandler;

    // add middlewares to server in invokation order
    using namespace std::placeholders;
    server.addRequestHandler(std::bind(&MyFileApi::handleRequest, &fileApiHandler, _1, _2));

    // uncomment to print debug message from server
    // server.setDebugMsgHandler([](const std::string& msg) { Serial.println(msg.c_str()); });

    // starts the asio::io_context and hence the server, this is blocking and
    // the reason we're running in a thread.
    ioc.run();
    Serial.println("Unexpected io_context termination");
}

// somewhere in setup() ..
    xTaskCreate(httpServerThread,   // Function that should be called
                "beauty",           // Name of the task (for debugging)
                8192,               // Stack size (bytes)
                NULL,               // Parameter to pass
                1,                  // Task priority
                NULL                // Task handle

```
# Execution order
For an incomming http request, Beauty first invokes the middleware stack in
added order. If no middleware respond to the request, Beauty will call the file
io handler (if defined).

If the file io handler fails to open the requested file, Beauty will respond
with 404. It is possible to "addFileNotFoundHandler" to provide custom 404
logic and response.

# Server
The Server is what runs on top of the Asio::io_context. It has two constructors,
one for PC and one for ESP32.

|Contructor |Description |
|--|--|
|`Server(asio::io_context &ioContext, uint16_t port, IFileIO *fileIO, HttpPersistence options, size_t maxContentSize = 1024)`| Use with ESP32|
|`Server(asio::io_context &ioContext, const std::string &address, const std::string &port, IFileIO *fileIO, HttpPersistence options, size_t maxContentSize = 1024)`| Use on PC|

|Constructor argument |Description |
|--|--|
|ioContext |The asio::io_context |
|address |Address of the network interface to use, PC constructor only |
|port |The port that the server binds and responds too.<br>**Note.** For the PC constructor this can be set to 0 in which case the operating system will assign a free port.|
|fileIO| The implementation class for IFileIO, see examples. May be set to nullptr of no file access is needed. |
|options| See HTTP persistence options below|
|maxContentSize| The max size in bytes of request/response buffers. Each connection will allocate one buffer for each direction. The minimum buffer size is 1024.|

|Methods |Description |
|--|--|
|`void addRequestHandler(const handlerCallback &cb)` | Adds custom middleware (web api) handlers. See examples.|
|`void setFileNotFoundHandler(const handlerCallback &cb)` | Adds a custom file not find handler. If not set, Beauty will provide a stock reply. |
|`void setDebugMsgHandler(const debugMsgCallback &cb)` | Adds a custom "printf" handler to get debug messages from Beauty. |

The definitions of `handlerCallback` and `debugMsgCallback` can be found in src/beauty_common.hpp.

## HTTP persistence options
Beauty support HTTP/1.1 using Keep-Alive connections.
The advantage of Keep-Alive connections is faster response time and avoid
unnecessary re-allocation of buffers for repeated request/response cycles with
the same clients.
The drawback is of coarse that memory may be used up more rapidly when serving
many clients. The `connectionLimit` may serve as a trade-off to achieve both
advantages for constrained environments.

The behaviour is controlled with HttpPersistence, defined in src/beauty_common.hpp.
It is required by both Server constructors and includes the following members:

|Variable |Description |
|--|--|
|`std::chrono::seconds keepAliveTimeout_`| Keep-Alive timeout for inactive connections. Sent in Keep-Alive response header.  0s = Keep-Alive disabled.<br>When disabled, Beauty acts as a HTTP/1.0 server, sending Connection=close in all responses. |
|`size_t keepAliveMax_` |Max number of request that can be processed on the connection before it is closed. Sent in Keep-Alive response header.<br>**Note.** Only relevant if keepAliveTimeout_ > 0s|
|`size_t connectionLimit_` |Internal limitation of the number of persistent http connections that are allowed. If this limit is exceeded, Connection=close will be sent in the response for new connections.<br>0 = no limit.<br>**Note.** Only relevant if keepAliveTimeout_ > 0s. |

# Middleware design
A middleware is defined by implementing the `handlerCallback` function. E.g. as:
```
void handleRequest(const Request &req, Reply &rep);
```
The example directory contains examples.

## The Request object
The Request object contains the parsed http request including parsed headers
and body data. It represents what the request looked like upon reception and
must never be modfied.

The following member variables provides the request information:
|Variable |Description |
|---|---|
|`std::string method_` |"GET"/"POST" etc.   |
|`std::string uri_` |e.g "/file.bin?myKey=my%20value"   |
|`int httpVersionMajor_` |e.g. 1  |
|`int httpVersionMinor_` |e.g. 1   |
|`std::vector<Header> headers_`  |Headers provided in request.  |
|`bool keepAlive_`  |Keep-alive status of the connection.  |
|`std::string requestPath_` |e.g. /file.bin |
|`std::vector<char> body_` |Body data of the request.   |

The following helper methods are provided:
|Method |Description |
|---|---|
|`Param getQueryParam(const std::string &key)` |Returns struct Param{bool exists_; std::string value;) }|
|`Param getFormParam(const std::string &key)` |Returns struct Param{bool exists_; std::string value;) }|
|`bool startsWith(const std::sting &sw)` |Return true if the requestPath_ starts with provided string.|

## The Reply object
The Reply object is what should be modified when a middleware acts on a request.

The following member variables can be modified in the reply.
|Variable | Description |
|---|---|
|`std::vector<char> content_`| The content data of the response (i.e. typically the response body). Only used with selected `send` method, see below. |
|`std::string filePath_`  |Intialized with Request::requestPath_. Can be modified by middleware before files io handler reads the file. See examples. |
|`std::string fileExtension_` |Beauty parses the file extension provided in the `Request::requestPath_` and stores it here for convenient access. |

The following methods are provided:
|Metod |Description |
|---|---|
|`void addHeader(const string &name, const string &value)`|Beauty always adds the `Content-Length` header automatically. So this header must never be added through addHeader().<br>Typically Beauty will also automatically add the `Content-Type` header. However if this method is used, Beauty will not add the `Content-Type` header.<br>So when using this method, all response headers (except for `Content-Length`), must be added.<br>This scheme allows to control the `Content-Type` header from a middleware in special cases.  See examples/pc/my_file_api.cpp. |
|`void send(status_type)` | Use when replying without a response body.|
|`void send(status_type, string contentType)` |Use with `Reply::content_`. `Reply::content_` must be loaded with the response body data before the send method is called.<br>**Note.** If combined with `addHeader()`, the contentType argument do add the `Content-Type` header.|
|`void send(status_type, string contentType, char* data, size_t size)`&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;  |Use when pointing to memory holding the response body data.<br>**Note.** If combined with `addHeader()`, the contentType argument do add the `Content-Type` header. |
|`void stockReply(status_code)`|Replies with a stock body for the status_code. |

