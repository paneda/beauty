# Beauty
> **A lightweight, powerful web server designed for constrained environments**

Beauty was born with the realization that Asio is supported on ESP32 microcontrollers, however Beauty runs anywhere Asio is supported, from tiny IoT devices to high-performance servers.

**Inspired by Express.js** - Familiar middleware pattern with predictable execution order  
**Born for Embedded** - Developed specifically for ESP32 but scales to any platform  
**Production Ready** - Used in real-world IoT deployments and web applications

## ✨ Key Features

| Feature | Description |
|---------|-------------|
| **HTTP/1.1 Support** | Full HTTP/1.1 with configurable persistent connections |
| **Multi-part Upload** | Handle large file uploads with customizable validation |
| **WebSocket Protocol** | RFC 6455 compliant real-time communication |
| **Flexible File System** | Adapt to any storage (LittleFS, SPIFFS, std::fstream) |
| **High Performance** | Asynchronous, lock-free, predictable memory usage |
| **Development Friendly** | Mock, develop and test on PC, deploy to embedded |

## 📋 Table of Contents

- [🌟 Beauty](#-beauty)
  - [✨ Key Features](#-key-features)
  - [📋 Table of Contents](#-table-of-contents)
  - [📦 Dependencies](#-dependencies)
  - [🚀 Quick Start](#-quick-start)
    - [💻 PC Development (Linux/Windows/macOS)](#-pc-development-linuxwindowsmacos)
    - [🔧 ESP32 Integration](#-esp32-integration)
  - [🏗️ Architecture](#️-architecture)
  - [⚙️ Server Configuration](#️-server-configuration)
    - [Constructor Options](#constructor-options)
    - [Server Methods](#server-methods)
    - [Settings & Limits](#settings--limits)
  - [🔧 Middleware Development](#-middleware-development)
    - [The Request Object](#the-request-object)
    - [The Reply Object](#the-reply-object)
  - [🚦 HTTP/1.1 100-continue Support](#-http11-100-continue-support)
  - [📊 HttpResult - JSON Made Easy](#-httpresult---json-made-easy)
  - [🛣️ Router - REST API Simplified](#️-router---rest-api-simplified)
  - [🔌 WebSocket Support](#-websocket-support)

## 📦 Dependencies
- **Asio (non-boost)** - Async I/O operations
- **>=C++11** - The core library is kept compatible with C++11 for maximum portability

## 🚀 Quick Start

### 💻 PC Development (Linux/Windows/macOS)

Get up and running in under 30 seconds:

```bash
# Clone and build
mkdir build && cd build
cmake --build . ..

# Launch with built-in demos
./build/examples/beauty_example 127.0.0.1 8080 www

# Open http://127.0.0.1:8080 and explore:
#   • Static file serving
#   • Low-level Web API
#   • Full JSON REST API
#   • Multi-part file uploads  
#   • WebSocket chat & data streaming
#   • Flow control demonstrations
```

> **Tip**: The the test_scripts folder can be used to  explore more advanced features like 100-continue.

### 🔧 ESP32 Integration

Beauty seamlessly integrates with both **PlatformIO/Arduino** and **ESP-IDF** frameworks. Since Asio's `io_context::run()` is blocking, we recommend using an RTOS thread for most applications:

> 🔧 **Framework Support**: Add Beauty as an external library following your framework's documentation

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
    beauty::Settings settings(std::chrono::seconds(5), 1000, 20);

    // the asio::context
    asio::io_context ioc;

    // must use an alternative constructor compared to PC example
    beauty::Server server(ioc, 80, settings, 1024);
    server.setFileIO(&fio);

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
## 🏗️ Architecture

Beauty follows a **middleware-first architecture** inspired by Express.js:

```
HTTP Request → Middleware Stack → File Handler → HTTP Response
               (in order added)     (fallback)
```

**Request Flow:**
1. 🔍 **Middleware Stack**: Executed in the order you add them
2. 📁 **File Handler**: Automatic fallback for static files (if configured)
3. ❌ **404/501**: Proper error responses if no handler matches

> 💡 **Tip**: Put your most frequently accessed routes first in the middleware stack for better performance!

## ⚙️ Server Configuration
Beauty's `Server` class runs on top of Asio's `io_context` and provides platform-optimized constructors:

### Constructor Options

| Platform | Constructor | Use Case |
|----------|-------------|----------|
| 🔧 **ESP32** | `Server(io_context, port, settings, maxContentSize)` | Embedded systems |
| 💻 **PC/Server** | `Server(io_context, address, port, settings, maxContentSize)` | Development & production |

### Constructor Parameters

| Parameter | Description | 💡 Tips |
|-----------|-------------|---------|
| `ioContext` | Asio's event loop engine | Shared across all async operations |
| `address` | Network interface (PC only) | Use `"0.0.0.0"` for all interfaces |
| `port` | TCP port to bind | Set to `0` for OS-assigned port |
| `settings` | HTTP persistence & timeouts | Configure for your use case |
| `maxContentSize` | Buffer size per connection | Min 1024 bytes, scale with needs |

> 🚀 **Performance Tip**: Each connection allocates 2 buffers (read/write) of `maxContentSize`. Plan your memory accordingly!

### Server Methods

| Method | Purpose | 💡 When to Use |
|--------|---------|----------------|
| `addRequestHandler(callback)` | Add middleware/API handlers | REST APIs, custom routing logic |
| `setFileIO(IFileIO*)` | Configure file system adapter | Static files, uploads, embedded storage |
| `setExpect100ContinueHandler(callback)` | Handle large upload validation | Auth checks and file size limitations before accepting big files |
| `setWsEndpoints(vector<shared_ptr<WsEndpoint>>)` | Register WebSocket endpoints | Real-time communication |
| `setDebugMsgHandler(callback)` | Custom debug message handler | Development debugging, production logging |

> 📚 **Reference**: Find callback definitions in `src/beauty/beauty_common.hpp`

### Settings & Limits

Beauty's `Settings` class gives you fine-grained control over resource usage and connection behavior - perfect for **constrained environments**:

```cpp
beauty::Settings settings(
    std::chrono::seconds(30),  // keepAliveTimeout
    100,                       // keepAliveMax  
    50                         // connectionLimit
);
```

| Setting | Purpose | 🎯 Optimization |
|---------|---------|-----------------|
| `keepAliveTimeout_` | HTTP Keep-Alive timeout | `0s` = HTTP/1.0 mode, `>0s` = HTTP/1.1 |
| `keepAliveMax_` | Max requests per connection | Balance connection reuse vs memory |
| `connectionLimit_` | Max concurrent connections | **Critical for embedded**: prevents OOM |
| `wsReceiveTimeout_` | WebSocket message timeout | Detect dead clients |
| `wsPingInterval_` | WebSocket ping frequency | Keep connections alive through NAT |
| `wsPongTimeout_` | WebSocket pong response timeout | Clean up unresponsive clients |

> 💾 **Memory Management**: `connectionLimit_` is your best friend on ESP32 - set it based on available RAM!  
> ⚡ **Performance**: Keep-Alive reduces connection overhead but uses more memory per client

## 🔧 Middleware Development

Middleware in Beauty is **beautifully simple** - just implement the `handlerCallback` function:

```cpp
void handleRequest(const beauty::Request& req, beauty::Reply& rep) {
    // Your magic happens here! ✨
    if (req.requestPath_ == "/api/hello") {
        rep.send(beauty::Reply::ok, "application/json", R"({"message": "Hello World!"})");
    }
}
```

> 🎯 **Design Philosophy**: One function, total control. Check out our examples for inspiration!

### The Request Object

The `Request` object is your **read-only window** into what the client sent. It's fully parsed and ready to use:

#### 📋 Core Properties

| Property | Example | Purpose |
|----------|---------|---------|
| `method_` | `"GET"`, `"POST"` | HTTP method |
| `uri_` | `"/api/users?page=1"` | Full URI with query string |
| `requestPath_` | `"/api/users"` | Clean path without query |
| `body_` | `std::vector<char>` | Request body data |
| `headers_` | `std::vector<Header>` | All HTTP headers |
| `keepAlive_` | `true`/`false` | Connection persistence |

#### 🛠️ Helper Methods

```cpp
// Parse query parameters: /api/users?page=1&limit=10
auto page = req.getQueryParam("page");    // page.exists_, page.value_
auto limit = req.getQueryParam("limit");

// Parse form data: Content-Type: application/x-www-form-urlencoded
auto username = req.getFormParam("username");

// Path matching: for simple routing
if (req.startsWith("/api/")) {
    // Handle API requests
}
```

> 🔒 **Immutable by Design**: Request objects are read-only to prevent accidental modifications

### The Reply Object

The `Reply` object is your **canvas for crafting responses**. Modify it to send exactly what your clients need:

#### 🎨 Response Building

| Method | Use Case | Example |
|--------|----------|---------|
| `send(status)` | Status-only responses | `rep.send(beauty::Reply::not_found)` |
| `send(status, contentType)` | With `content_` buffer | JSON, HTML, custom data |
| `send(status, contentType, data, size)` | Direct memory pointer | Large files |

#### 🔧 Advanced Control

```cpp
// Custom headers (takes full control)
rep.addHeader("Cache-Control", "no-cache");
rep.addHeader("Content-Type", "application/json");
// Note: When using addHeader(), you control ALL headers except Content-Length and Connection

// File handling magic
rep.filePath_ = "/custom/path.html";        // Redirect file requests
rep.fileExtension_ = ".json";               // Override detected extension
```

#### 💡 Real-World Examples

```cpp
// JSON API Response
rep.content_ = R"({"users": [], "total": 0})";
rep.send(beauty::Reply::ok, "application/json");

// Custom Error with CORS
rep.addHeader("Access-Control-Allow-Origin", "*");
rep.addHeader("Content-Type", "text/plain");
rep.content_ = "API key required";
rep.send(beauty::Reply::unauthorized);

// File Download
rep.addHeader("Content-Disposition", "attachment; filename=data.csv");
rep.send(beauty::Reply::ok, "text/csv", csvData.data(), csvData.size());
```

> ⚡ **Tip**: Use `content_` for dynamic data, direct pointers for static/large files


## 🚦 HTTP/1.1 100-continue Support
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

## 📊 HttpResult - JSON Made Easy
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
## 🛣️ Router - REST API Simplified

Beauty provides a lightweight, router implementation. It avoids heavy dependencies like `std::regex` and uses string operations for path matching. The router provides some key features listed below but is not required when implementing a Web API request handler.

### Features

- **Lightweight**: No regex dependencies, uses simple string operations
- **Embedded-friendly**: Minimal memory footprint and predictable performance
- **Path Parameters**: Supports parameterized paths like `/users/{userId}`
- **Multiple HTTP Methods**: Support for GET, POST, PUT, DELETE, etc.
- **HTTP/1.1 Compliant**: Full compliance with HTTP/1.1 specification
- **CORS Support**: Complete Cross-Origin Resource Sharing implementation
- **Automatic HEAD Support**: HEAD requests automatically supported for all GET routes
- **OPTIONS Method**: Automatic method discovery and CORS preflight handling
- **Proper Error Responses**: 405 Method Not Allowed with Allow header
- **Easy Integration**: Designed to work seamlessly with Beauty's request handling

### HTTP/1.1 Compliance & CORS

The router provides comprehensive HTTP/1.1 compliance and CORS support:

#### HTTP/1.1 Features
- **HEAD Method**: Automatically supported for all GET routes
- **OPTIONS Method**: Returns allowed methods for any resource
- **405 Responses**: Method Not Allowed responses include proper Allow header
- **Proper Status Codes**: Compliant with HTTP/1.1 specification

#### CORS Support
```cpp
// Configure CORS for cross-origin requests
beauty::CorsConfig corsConfig;
corsConfig.allowedOrigins.insert("https://myapp.com");
corsConfig.allowedOrigins.insert("http://localhost:3000");
corsConfig.allowedHeaders.insert("Authorization");
corsConfig.allowCredentials = true;
corsConfig.maxAge = 3600; // 1 hour preflight cache

router.configureCors(corsConfig);
```

**CORS Features:**
- **Preflight Handling**: Automatic CORS preflight request processing
- **Origin Validation**: Configurable allowed origins (including wildcards)
- **Header Control**: Specify allowed and exposed headers
- **Credentials Support**: Configurable cookie/auth header support
- **Cache Control**: Configurable preflight response caching

**What CORS Enables:**
- Modern web applications with separate frontend/backend
- Cross-domain API access from browsers
- Integration with JavaScript frameworks (React, Vue, Angular)
- Web based mobile apps (e.g. PWA)

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
// Using the Router, the request handler is implemented as:
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
- CORS configuration for cross-origin requests
- HTTP/1.1 compliance features (HEAD, OPTIONS, 405 responses)
- Cross-origin testing examples with curl commands
- Integration with Beauty's HttpResult

### Performance Characteristics

- **Memory**: Minimal overhead, stores only parsed route segments and CORS config
- **CPU**: Simple string comparisons, no regex compilation or matching
- **Predictable**: Linear time complexity O(n) where n is the number of route segments
- **Embedded-friendly**: No dynamic regex compilation, fixed memory usage per route
- **CORS Efficient**: CORS headers only added when needed (cross-origin requests)
- **HTTP/1.1 Compliance**: HEAD and OPTIONS responses generated without handler execution

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

## 🔌 WebSocket Support

Beauty provides WebSocket support in compliance with RFC 6455.
The API provides the primitives to build custom queue and application-controlled back-pressure handling.
The WebSocket connections reuse the existing HTTP connection buffers, making the memory overhead minimal:

## Key Features

- **RFC 6455 Compliant**: Full WebSocket protocol implementation
- **Path-based Endpoints**: Support for multiple WebSocket endpoints on different paths
- **Customizable Flow Control**: Allows for advanced back-pressure handling
- **Thread-safe**: Built on Asio's single-threaded event loop model
- **Connection Management**: Automatic connection lifecycle management
- **Ping/Pong Handling**: Built-in connection health monitoring

## Basic WebSocket Setup

### 1. Create WebSocket Endpoints

WebSocket functionality is implemented through endpoint classes that inherit from `WsEndpoint`:

```cpp
#include "beauty/ws_endpoint.hpp"

class MyChatEndpoint : public beauty::WsEndpoint {
public:
    MyChatEndpoint() : WsEndpoint("/ws/chat") {}

    void onWsOpen(const std::string& connectionId) override {
        sendText(connectionId, "Welcome to the chat!");
    }

    void onWsMessage(const std::string& connectionId, const beauty::WsMessage& message) override {
        // Echo message to all connected clients
        std::string msg(message.content_.begin(), message.content_.end());
        for (const auto& connId : getActiveConnections()) {
            sendText(connId, "User " + connectionId + ": " + msg);
        }
    }

    void onWsClose(const std::string& connectionId) override {
        for (const auto& connId : getActiveConnections()) {
            sendText(connId, "User " + connectionId + " has left the chat.");
        }
    }

    void onWsError(const std::string& connectionId, const std::string& error) override {
        // Handle connection errors
    }
};
```

### 2. Register Endpoints with Server

```cpp
// Create endpoints
auto chatEndpoint = std::make_shared<MyChatEndpoint>();
auto dataEndpoint = std::make_shared<MyDataEndpoint>();

// Register with server
server.setWsEndpoints({chatEndpoint, dataEndpoint});
```

### 3. Client Connection

Clients connect to specific WebSocket endpoints using their configured paths:

```javascript
// Connect to chat endpoint
const chatSocket = new WebSocket('ws://localhost:8080/ws/chat');

// Connect to data endpoint  
const dataSocket = new WebSocket('ws://localhost:8080/ws/data');
```

## WebSocket Endpoint API

### Core Methods (Override These)

```cpp
class MyEndpoint : public beauty::WsEndpoint {
public:
    // Called when a client connects
    void onWsOpen(const std::string& connectionId) override;
    
    // Called when a message is received
    void onWsMessage(const std::string& connectionId, const beauty::WsMessage& message) override;
    
    // Called when a client disconnects
    void onWsClose(const std::string& connectionId) override;
    
    // Called on connection errors
    void onWsError(const std::string& connectionId, const std::string& error) override;
};
```

### Sending Messages (Use These)

```cpp
// Send to specific connection
bool sendText(const std::string& connectionId, const std::string& message);
bool sendBinary(const std::string& connectionId, const std::vector<char>& data);
bool sendClose(const std::string& connectionId, uint16_t statusCode = 1000, const std::string& reason = "");

// Get connection information
std::vector<std::string> getActiveConnections() const;
```

## Advanced Flow Control

For production applications that need to handle varying client performance or bursty data producers,
Beauty allows writing advanced flow control managment to prevent memory buildup and maintain system responsiveness.

### Write State Tracking

```cpp
// Check if connection can send (not in middle of write operation)
bool canSendTo(const std::string& connectionId) const;

// Send with optional callbacks for flow control or monitoring
WriteResult sendText(const std::string& connectionId, 
                     const std::string& message, 
                     WriteCompleteCallback callback);

WriteResult sendBinary(const std::string& connectionId, 
                       const std::vector<char>& data, 
                       WriteCompleteCallback callback);
```

### Flow Control Strategies (Examples)

Beauty provides building blocks for flow control rather than making policy decisions. Here are common strategies:
See examples/pc/my_data_streaming_endpoint.cpp for a complete working example.

#### 1. Drop-on-Busy (Real-time Data)
```cpp
void broadcastData(const std::string& data) {
    for (const auto& connId : getActiveConnections()) {
        if (canSendTo(connId)) {
            sendText(connId, data);
        } else {
            // Drop message for slow clients
            statisticsDropped_[connId]++;
        }
    }
}
```

#### 2. Queue with Limits (Important Messages)
```cpp
void sendImportantMessage(const std::string& connId, const std::string& msg) {
    if (canSendTo(connId)) {
        sendText(connId, msg);
    } else if (messageQueue_[connId].size() < MAX_QUEUE_SIZE) {
        messageQueue_[connId].push(msg);
    } else {
        // Queue full, handle accordingly
        handleQueueFull(connId);
    }
}
```

#### 3. Adaptive Rate Limiting
```cpp
void adaptiveSend(const std::string& connId, const std::string& data) {
    auto& stats = connectionStats_[connId];
    
    if (canSendTo(connId) || stats.getDropRate() < 0.1) {  // Less than 10% drop rate
        auto result = sendText(connId, data, 
            [&stats](const std::error_code& ec, std::size_t bytes) {
                if (!ec) stats.messagesSent++;
            });
        
        if (result == WriteResult::WRITE_IN_PROGRESS) {
            stats.messagesDropped++;
        }
    } else {
        stats.messagesDropped++;
    }
}
```

### Write Result Codes

```cpp
enum class WriteResult {
    SUCCESS,           // Message queued for sending
    WRITE_IN_PROGRESS, // Connection is busy with another write
    CONNECTION_CLOSED  // Connection is closed or invalid
};
```

## Protocol Details

### WebSocket Handshake
Beauty automatically handles the WebSocket handshake process:
1. Client sends HTTP Upgrade request with WebSocket headers
2. Beauty validates the request and generates proper Sec-WebSocket-Accept header
3. HTTP 101 Switching Protocols response is sent
4. Connection is upgraded to WebSocket protocol

### Frame Types Supported
- **Text frames**: UTF-8 encoded text messages
- **Binary frames**: Raw binary data
- **Close frames**: Connection termination with status codes
- **Ping/Pong frames**: Automatic connection health monitoring

### Connection Management
- Automatic ping/pong handling for connection health
- Configurable timeouts and limits
- Graceful connection shutdown
- Error handling and recovery

## Memory Usage

WebSocket connections reuse the existing HTTP connection buffers, making the memory overhead minimal:
- Each WebSocket connection uses the same `maxContentSize` buffer as HTTP
- No additional per-connection memory allocation for WebSocket protocol
- Efficient buffer reuse for frame parsing and encoding

## Testing
Beauty includes a comprehensive WebSocket test interface at `/ws/chat` and `/ws/data` that allows:
- Testing multiple endpoint connections simultaneously
- Flow control simulation with bursty data
- Real-time statistics monitoring
- Interactive message sending and broadcasting

