# Beauty
Beauty is a web server designed for constrained environments. In particular it
was developed to run on ESP32 that provide Asio support.
However it will run in any environment that supports Asio (non-boost).

It is insipired by Express.js in that it executes a stack of middlewares that
are executed in added order. See examples.

Its main properties:
* Supports HTTP/1.1 (configurable support for persistent connections)
* Multi-part file upload
* 100-continue support
* Adaptable to any file system (e.g. LittleFs on ESP32 or std::fstream)
* Fast, asynchronous and lock-free implementation
* Low and predictable heap memory requirement (configurable support of buffer sizes)
* No ESP-IDF/Arduino dependencies, i.e. the web application can be mocked and
  tested on PC during development

# Dependencies
* Asio (non-boost)
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
Beauty can be used in both PlatformIO/Arduino and ESP-IDF development frameworks.
Check documentation in respective framework how to add external libraries.

As asio::io_context::run() is blocking, the code below uses a RTOS thread as
this is probably the best fit for most applications. However if nothing else
needs to run in loop(), the RTOS thread can be omitted and the code below can
be simplified to the "standard" Arduino setup() and loop() concept.

```
// included for this example
#include <functional>
#include <chrono>
#include <Arduino.h>
#include <WiFi.h>
#include <asio.hpp>
#include <beauty/server.hpp>
#include <beauty/reply.hpp>

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

The definitions of `handlerCallback` and `debugMsgCallback` can be found in src/beauty/beauty_common.hpp.

## HTTP persistence options
Beauty support HTTP/1.1 using Keep-Alive connections.
The advantage of Keep-Alive connections is faster response time and avoid
unnecessary re-allocation of buffers for repeated request/response cycles from
the same clients.
The drawback is of coarse that memory may be used up more rapidly when serving
many clients. The `connectionLimit` may serve as a trade-off to achieve both
advantages for constrained environments.

The behaviour is controlled with HttpPersistence, defined in src/beauty/beauty_common.hpp.
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


## HTTP/1.1 100-continue Support
The 100-continue mechanism allows HTTP clients to send request headers first,
wait for server approval (100 Continue response), and only then send the
potentially large request body. This is particularly useful for:

- Authentication/authorization validation before accepting large uploads
- Content-Type validation
- Content-Length limits checking
- Bandwidth optimization by avoiding unnecessary data transfer

Beauty supports 100-continue as described in RFC 7231, section 5.1.1. By
default, Beauty will approve all requests with the `Expect: 100-continue` header.
However, a custom handler can be set to implement application-specific logic to
approve or reject such requests. A handler can e.g. check for valid
authentication tokens, validate content types, or enforce content length
limits.

A custom 100-continue handler can be set using the `setExpectContinueHandler`
method of the `Server` class. For simplicity, the handler have the same signature as
a request handler. I.e. the handler should send the 200 OK status code in the
`Reply` object if the request is approved, or an appropriate error status code.

The handler may be registered as follows:

```cpp
server.setExpectContinueHandler([](const beauty::Request& req, beauty::Reply& rep) -> void {
    // Validate headers before accepting body
    std::string auth = req.getHeaderValue("Authorization");
    if (auth.empty()) {
        rep.send(beauty::Reply::status_type::bad_request);
    }
    // Check content length
    if (req.getContentLength() > MAX_ALLOWED_FILE_SIZE) {
        rep.send(beauty::Reply::status_type::payload_too_large);
    }
    // Approve the request
    rep.send(beauty::Reply::status_type::ok); // Actually sends 100 Continue
});
```

## HttpResult (optional)
As the request and reply classes store data in `std::vector<char>` it becomes
a bit hard to manipulate their data as e.g. JSON documents. Therefore, the
HttpResult class provides a convenient way to do so.

For high portability and low footprint, HttpResult uses cJSON, so cJSON needs
to be imported to your project. If you have a different preferred JSON library,
you may use HttpResult as inspiration.

### Constructor
HttpResult takes a reference to the `Reply::content_` buffer:
```cpp
HttpResult res(rep.content_);
```

### Parsing JSON requests
```cpp
// Parse JSON from request body
if (res.parseJsonRequest(req.body_)) {
    // Access parsed values with type safety
    std::string name = res.getString("name", "default");
    int age = res.getInt("age", 0);
    bool active = res.getBool("active", false);
    double score = res.getDouble("score", 0.0);
    
    // Check if key exists
    if (res.containsKey("email")) {
        std::string email = res.getString("email");
    }
}
```

### Advanced JSON parsing with cJSON
For complex nested JSON structures, arrays, or when you need to use advanced cJSON functions directly, use the `getRoot()` method:
```cpp
// Parse complex nested JSON
if (res.parseJsonRequest(req.body_)) {
    // Get direct access to cJSON root for advanced operations
    cJSON* root = res.requestBody_.getRoot();
    
    // Navigate nested objects
    cJSON* user = cJSON_GetObjectItem(root, "user");
    if (user && cJSON_IsObject(user)) {
        cJSON* address = cJSON_GetObjectItem(user, "address");
        if (address && cJSON_IsObject(address)) {
            cJSON* street = cJSON_GetObjectItem(address, "street");
            if (street && cJSON_IsString(street)) {
                std::string streetName = street->valuestring;
            }
        }
    }
    
    // Process arrays
    cJSON* items = cJSON_GetObjectItem(root, "items");
    if (items && cJSON_IsArray(items)) {
        int arraySize = cJSON_GetArraySize(items);
        for (int i = 0; i < arraySize; i++) {
            cJSON* item = cJSON_GetArrayItem(items, i);
            if (item && cJSON_IsObject(item)) {
                cJSON* id = cJSON_GetObjectItem(item, "id");
                if (id && cJSON_IsNumber(id)) {
                    int itemId = id->valueint;
                    // Process item...
                }
            }
        }
    }
}
```

### Building JSON responses
Simple key-value responses:
```cpp
// Single string value
res.singleJsonKeyValue("message", "Hello World");

// Single numeric value  
res.singleJsonKeyValue("count", 42);

// Single boolean value
res.singleJsonKeyValue("success", true);
```

Complex JSON structures:
```cpp
// Build custom JSON response
res.buildJsonResponse([&]() -> cJSON* {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "ok");
    cJSON_AddNumberToObject(root, "timestamp", time(nullptr));
    
    cJSON* users = cJSON_CreateArray();
    for (const auto& user : userList) {
        cJSON* userObj = cJSON_CreateObject();
        cJSON_AddStringToObject(userObj, "name", user.name.c_str());
        cJSON_AddNumberToObject(userObj, "id", user.id);
        cJSON_AddItemToArray(users, userObj);
    }
    cJSON_AddItemToObject(root, "users", users);
    
    return root;
});
```

Direct array responses:
```cpp
// Return JSON array as root element
res.buildJsonResponse([&]() -> cJSON* {
    cJSON* filesArray = cJSON_CreateArray();
    for (const auto& file : files) {
        cJSON* fileObj = cJSON_CreateObject();
        cJSON_AddStringToObject(fileObj, "name", file.name.c_str());
        cJSON_AddNumberToObject(fileObj, "size", file.size);
        cJSON_AddItemToArray(filesArray, fileObj);
    }
    return filesArray;
});
```

Error responses:
```cpp
// Generate error response with status code
res.jsonError(Reply::status_type::bad_request, "Invalid input data");
```

### Sending JSON responses
After building the JSON response, send it:
```cpp
rep.send(res.statusCode_, "application/json");
```
### Building non-JSON responses
For HTML, plain text, or other non-JSON responses, use the streaming operator:
```cpp
// HTML response
res << "<!DOCTYPE html>"
    << "<html><head><title>My Page</title></head>"
    << "<body>"
    << "<h1>Welcome to Beauty Server</h1>"
    << "<p>Current time: " << getCurrentTime() << "</p>"
    << "</body></html>";

rep.send(res.statusCode_, "text/html");
```

```cpp
// Plain text response
res << "Server status: OK\n"
    << "Uptime: " << getUptime() << " seconds\n"
    << "Active connections: " << getConnectionCount();

rep.send(res.statusCode_, "text/plain");
```

```cpp
// CSV response
res << "Name,Age,City\n"
    << "John,25,New York\n"
    << "Jane,30,London\n";

rep.send(res.statusCode_, "text/csv");
```
## Router (optional)

Beauty provides a lightweight, router implementation. It avoids heavy dependencies like `std::regex` and uses string operations for path matching.

### Features

- **Lightweight**: No regex dependencies, uses simple string operations
- **Embedded-friendly**: Minimal memory footprint and predictable performance
- **Path Parameters**: Supports parameterized paths like `/users/{userId}`
- **Multiple HTTP Methods**: Support for GET, POST, PUT, DELETE, etc.
- **Easy Integration**: Designed to work seamlessly with Beauty's request handling

### Basic Usage

#### 1. Include the Router

```cpp
#include "beauty/router.hpp"
```

#### 2. Create and Configure Router

```cpp
beauty::Router router;

// Add routes
router.addRoute("GET", "/users", 
    [](const beauty::Request& req, beauty::Reply& rep, const std::unordered_map<std::string, std::string>& params) {
        // Handle GET /users
    });

router.addRoute("GET", "/users/{userId}", 
    [](const beauty::Request& req, beauty::Reply& rep, const std::unordered_map<std::string, std::string>& params) {
        std::string userId = params.at("userId");
        // Handle GET /users/{userId}
    });
```

#### 3. Handle Requests

```cpp
// Using the Router, a request handler is essentially implemented like this:
void handleRequest(const beauty::Request& req, beauty::Reply& rep) {
    if (router.handle(req, rep) == HandlerResult::Matched) {
        return; // Request was handled by router
    }
}
```

### Path Patterns

The router supports simple path patterns with parameters:

- `/users` - Exact match
- `/users/{userId}` - Match with parameter
- `/users/{userId}/posts/{postId}` - Multiple parameters
- `/api/v1/users/{userId}` - Mixed literal and parameter segments

#### Parameter Extraction

Parameters are automatically extracted and passed to handlers:

```cpp
router.addRoute("GET", "/users/{userId}/posts/{postId}", 
    [](const beauty::Request& req, beauty::Reply& rep, const std::map<std::string, std::string>& params) {
        std::string userId = params.at("userId");
        std::string postId = params.at("postId");
        // Use the parameters...
    });
```

### Complete Example

See `my_router_api.cpp` for a complete working example that demonstrates:

- Setting up multiple routes with different HTTP methods
- Handling path parameters
- Returning JSON responses
- Integration with Beauty's HttpResult

### Performance Characteristics

- **Memory**: Minimal overhead, stores only parsed route segments
- **CPU**: Simple string comparisons, no regex compilation or matching
- **Predictable**: Linear time complexity O(n) where n is the number of route segments
- **Embedded-friendly**: No dynamic regex compilation, fixed memory usage per route

### Building and Testing

The router is included in the Beauty examples CMake configuration. To build:

```bash
mkdir build && cd build
cmake -DBUILD_EXAMPLES=On ..
make
```

Start the example server and test the '/api/users' paths:

```bash
./build/examples/beauty_example 127.0.0.1 8080 www/
```