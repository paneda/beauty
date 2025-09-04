# HTTP/1.1 100-continue Support Implementation

This document describes the implementation of HTTP/1.1 100-continue support in the Beauty HTTP server library.

## Overview

The 100-continue mechanism allows HTTP clients to send request headers first, wait for server approval (100 Continue response), and only then send the potentially large request body. This is particularly useful for:

- Authentication/authorization validation before accepting large uploads
- Content-Type validation
- Content-Length limits checking
- Bandwidth optimization by avoiding unnecessary data transfer

## Implementation Details

### 1. Parser Changes

**RequestParser** (`src/beauty/request_parser.hpp`, `src/request_parser.cpp`):
- Added new result type: `good_headers_expect_continue`
- Added detection of `Expect: 100-continue` header in `storeHeaderValueIfNeeded()`
- Modified `checkRequestAfterAllHeaders()` to return the new result type when appropriate

**Request** (`src/beauty/request.hpp`):
- Added private field `expectContinue_` to track if request expects 100-continue
- Added public getter method `expectsContinue()` for user access
- Updated `reset()` method to clear the flag

### 2. Handler Infrastructure

**beauty_common.hpp**:
- Added new callback type: `expectContinueCallback = std::function<bool(unsigned connectionId, const Request &req)>`

**RequestHandler** (`src/beauty/request_handler.hpp`, `src/request_handler.cpp`):
- Added `setExpectContinueHandler(const expectContinueCallback &cb)` method
- Added `shouldContinueAfterHeaders(unsigned connectionId, const Request &req)` method
- Added private member `expectContinueCb_` to store the callback
- Default behavior: approve all 100-continue requests if no handler is set

**Server** (`src/beauty/server.hpp`, `src/server.cpp`):
- Added `setExpectContinueHandler(const expectContinueCallback &cb)` method that delegates to RequestHandler

### 3. Connection Handling

**Connection** (`src/beauty/connection.hpp`, `src/connection.cpp`):
- Added `doWrite100Continue()` method to send "HTTP/1.1 100 Continue\r\n\r\n" response
- Added `doReadBodyAfter100Continue()` method for reading body after 100-continue approval
- Modified `doRead()` to handle `good_headers_expect_continue` parser result
- Enhanced flow to preserve headers during 100-continue process

## Usage Example

```cpp
#include "beauty/server.hpp"

// Set up 100-continue handler
server.setExpectContinueHandler([](unsigned connectionId, const beauty::Request& req) -> bool {
    // Validate headers before accepting body
    std::string auth = req.getHeaderValue("Authorization");
    if (auth.empty()) {
        return false; // Reject - sends 400 Bad Request
    }
    
    // Check content length
    std::string contentLength = req.getHeaderValue("Content-Length");
    if (!contentLength.empty() && std::stoul(contentLength) > MAX_SIZE) {
        return false; // Reject
    }
    
    return true; // Approve - sends 100 Continue
});

// Normal request handlers work as before
server.addRequestHandler([](const beauty::Request& req, beauty::Reply& reply) {
    if (req.expectsContinue()) {
        // This request used 100-continue
        std::cout << "Body size: " << req.body_.size() << " bytes" << std::endl;
    }
    // Process request normally...
});
```

## Request Flow

### Standard Request (no 100-continue)
1. Client sends headers + body
2. RequestParser returns `good_complete` or `good_part`
3. RequestHandler processes complete request
4. Server sends response

### 100-continue Request
1. Client sends headers with `Expect: 100-continue`
2. RequestParser returns `good_headers_expect_continue`
3. Connection calls `RequestHandler::shouldContinueAfterHeaders()`
4. If approved: Connection sends "100 Continue" and waits for body
5. If rejected: Connection sends error response (400 Bad Request)
6. Client sends body (if approved)
7. Connection reads complete body using `doReadBodyAfter100Continue()`
8. RequestHandler processes complete request with headers + body
9. Server sends final response

## Backward Compatibility

This implementation is fully backward compatible:
- Existing code continues to work without changes
- 100-continue support is opt-in via `setExpectContinueHandler()`
- Default behavior approves all 100-continue requests
- Non-100-continue requests follow the same path as before

## Testing

The implementation includes tests in `test/request_parser_test.cpp` that verify:
- Correct parsing of `Expect: 100-continue` header
- Proper return of `good_headers_expect_continue` result type
- Setting of `expectContinue_` flag in Request object

## HTTP/1.1 Compliance

This implementation follows RFC 7231 Section 5.1.1 for the Expect header:
- Only responds to `Expect: 100-continue` in HTTP/1.1+ requests
- Sends proper "100 Continue" status line
- Handles rejection with appropriate error responses
- Maintains connection state correctly during the process
