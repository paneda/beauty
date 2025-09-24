#include <beauty/http_result.hpp>
#include <beauty/mime_types.hpp>

#include "my_router_api.hpp"

using namespace beauty;

/*
 * Example Router API with CORS Support
 *
 * This example demonstrates:
 * 1. Basic REST API routing (GET, POST, PUT, DELETE)
 * 2. Path parameters ({userId}, {postId})
 * 3. CORS configuration for cross-origin requests
 * 4. Automatic HEAD support for GET routes
 * 5. Automatic OPTIONS support for discovering allowed methods
 *
 * CORS Features:
 * - Configurable allowed origins, headers, and credentials
 * - Automatic preflight request handling
 * - CORS headers added to all responses
 * - Compliant with CORS specification (RFC 6454)
 *
 * HTTP/1.1 Compliance:
 * - Standard HTTP methods supported
 * - Proper status codes and headers
 * - HEAD requests automatically supported
 * - OPTIONS requests for method discovery
 * - 405 Method Not Allowed with Allow header
 */

// Example usage of the Router with CORS support
// This shows how to integrate the router with Beauty web server
MyRouterApi::MyRouterApi() {
    setupCorsConfiguration();
    setupRoutes();
}

void MyRouterApi::setupCorsConfiguration() {
    // Configure CORS settings for cross-origin requests
    beauty::CorsConfig corsConfig;

    // Allow specific origins (replace with your actual frontend domains)
    corsConfig.allowedOrigins.insert("http://localhost:3000");  // React dev server
    corsConfig.allowedOrigins.insert("http://localhost:8080");  // Vue dev server
    corsConfig.allowedOrigins.insert("https://myapp.com");      // Production domain
    corsConfig.allowedOrigins.insert("https://www.myapp.com");  // Production www domain

    // For public web api:s, without authorization requirements, you might want
    // to allow all origins (use with caution!)
    // Do NOT use "*" if your API supports credentials (cookies, Authorization header, etc.)
    // Browsers will block credentialed requests with wildcard origins for security reasons.
    // corsConfig.allowedOrigins.insert("*");

    // IMPORTANT: Only include NON-SAFELISTED headers here!
    // Safelisted headers (Accept, Content-Type, Accept-Language, Content-Language)
    // are automatically allowed and should NOT be included in this configuration.
    //
    // Common non-safelisted headers that APIs typically need:
    corsConfig.allowedHeaders.insert("Authorization");     // JWT tokens, API keys
    corsConfig.allowedHeaders.insert("X-Requested-With");  // Often sent by AJAX libraries
    corsConfig.allowedHeaders.insert("X-Api-Key");         // Custom API keys
    corsConfig.allowedHeaders.insert("X-Client-Version");  // Client version tracking
    corsConfig.allowedHeaders.insert("If-Match");          // ETags for optimistic concurrency
    corsConfig.allowedHeaders.insert("If-None-Match");     // ETags for caching

    // Expose response headers to JavaScript clients
    // By default, browsers only allow JS to access basic response headers.
    // Add custom response headers here that your client-side code needs to read:
    corsConfig.exposedHeaders.insert("X-Total-Count");    // For pagination (client reads this)
    corsConfig.exposedHeaders.insert("X-Rate-Limit");     // Rate limiting info (client reads this)
    corsConfig.exposedHeaders.insert("X-Request-Id");     // Request tracking (client reads this)
    corsConfig.exposedHeaders.insert("X-Response-Time");  // Performance metrics (client reads this)

    // Allow credentials (cookies, authorization headers)
    // Note: Cannot use wildcard origin (*) when credentials are allowed
    corsConfig.allowCredentials = true;

    // Set preflight cache duration (how long browsers cache OPTIONS responses)
    corsConfig.maxAge = 3600;  // 1 hour

    // Apply CORS configuration to the router
    router_.configureCors(corsConfig);
}

void MyRouterApi::setupRoutes() {
    // Route: GET /users
    router_.addRoute("GET",
                     "/api/users",
                     [this](const Request& req,
                            Reply& rep,
                            const std::unordered_map<std::string, std::string>& params) {
                         this->usersGet(req, rep, params);
                     });

    // Route: GET /users/{userId}
    router_.addRoute("GET",
                     "/api/users/{userId}",
                     [this](const Request& req,
                            Reply& rep,
                            const std::unordered_map<std::string, std::string>& params) {
                         this->usersUserIdGet(req, rep, params);
                     });

    // Route: POST /users
    router_.addRoute("POST",
                     "/api/users",
                     [this](const Request& req,
                            Reply& rep,
                            const std::unordered_map<std::string, std::string>& params) {
                         this->usersPost(req, rep, params);
                     });

    // Route: PUT /users/{userId}
    router_.addRoute("PUT",
                     "/api/users/{userId}",
                     [this](const Request& req,
                            Reply& rep,
                            const std::unordered_map<std::string, std::string>& params) {
                         this->usersUserIdPut(req, rep, params);
                     });

    // Route: DELETE /users/{userId}
    router_.addRoute("DELETE",
                     "/api/users/{userId}",
                     [this](const Request& req,
                            Reply& rep,
                            const std::unordered_map<std::string, std::string>& params) {
                         this->usersUserIdDelete(req, rep, params);
                     });

    // Route: GET /users/{userId}/posts/{postId}
    router_.addRoute("GET",
                     "/api/users/{userId}/posts/{postId}",
                     [this](const Request& req,
                            Reply& rep,
                            const std::unordered_map<std::string, std::string>& params) {
                         this->usersUserIdPostsPostIdGet(req, rep, params);
                     });

    // Route: GET /api/info - Demonstrates CORS headers and server info
    router_.addRoute("GET",
                     "/api/info",
                     [this](const Request& req,
                            Reply& rep,
                            const std::unordered_map<std::string, std::string>& params) {
                         this->apiInfoGet(req, rep, params);
                     });
}

// Main handler that Beauty will call
void MyRouterApi::handleRequest(const Request& req, Reply& rep) {
    auto result = router_.handle(req, rep);
    if (result == HandlerResult::Matched) {
        return;  // Request was handled by router

        // CORS is automatically handled by the router:
        // 1. CORS preflight requests (OPTIONS with CORS headers) are handled automatically
        // 2. CORS headers are added to all successful responses from registered routes
        //
        // HTTP/1.1 compliance:
        // 3. HEAD requests are automatically supported for all GET routes
        // 4. OPTIONS requests return the allowed methods for each endpoint
        // 5. 405 Method Not Allowed responses include the Allow header
    }

    // If no route matched, let another handler or finally FileIO handle the request.
    // Beauty will return 404 Not Found if no other handler matches.
}

// Handler implementations
void MyRouterApi::usersGet(const Request& /*req*/,
                           Reply& rep,
                           const std::unordered_map<std::string, std::string>& /*params*/) {
    HttpResult result(rep.content_);
    result.buildJsonResponse([&]() -> cJSON* {
        cJSON* usersArray = cJSON_CreateArray();
        // Add sample users
        cJSON* user1 = cJSON_CreateObject();
        cJSON_AddStringToObject(user1, "id", "1");
        cJSON_AddStringToObject(user1, "name", "John Doe");
        cJSON_AddItemToArray(usersArray, user1);

        cJSON* user2 = cJSON_CreateObject();
        cJSON_AddStringToObject(user2, "id", "2");
        cJSON_AddStringToObject(user2, "name", "Jane Smith");
        cJSON_AddItemToArray(usersArray, user2);

        return usersArray;
    });

    rep.addHeader("Cache-Control", "no-store");  // No caching for dynamic API

    rep.send(Reply::status_type::ok, "application/json");
}

void MyRouterApi::usersUserIdGet(const Request& /*req*/,
                                 Reply& rep,
                                 const std::unordered_map<std::string, std::string>& params) {
    HttpResult result(rep.content_);
    std::string userId = params.at("userId");

    result.buildJsonResponse([&]() -> cJSON* {
        cJSON* user = cJSON_CreateObject();
        cJSON_AddStringToObject(user, "id", userId.c_str());
        cJSON_AddStringToObject(user, "name", ("User " + userId).c_str());
        cJSON_AddStringToObject(user, "email", ("user" + userId + "@example.com").c_str());
        return user;
    });

    rep.addHeader("Cache-Control", "no-store");  // No caching for dynamic API
                                                 //
    rep.send(Reply::status_type::ok, "application/json");
}

void MyRouterApi::usersPost(const Request& req,
                            Reply& rep,
                            const std::unordered_map<std::string, std::string>& /*params*/) {
    HttpResult result(rep.content_);

    if (!result.parseJsonRequest(req.body_)) {
        result.jsonError(Reply::status_type::bad_request, "Invalid JSON in request");
        rep.send(result.statusCode_, "application/json");
        return;
    }

    // Here we would get the user properties here with
    // result.getSingleValue("name"), etc. and create the user

    result.buildJsonResponse([&]() -> cJSON* {
        cJSON* response = cJSON_CreateObject();
        cJSON_AddStringToObject(response, "message", "User created successfully");
        cJSON_AddStringToObject(response, "id", "123");
        return response;
    });
    rep.send(Reply::status_type::created, "application/json");
}

void MyRouterApi::usersUserIdPut(const Request& req,
                                 Reply& rep,
                                 const std::unordered_map<std::string, std::string>& params) {
    HttpResult result(rep.content_);
    std::string userId = params.at("userId");

    if (!result.parseJsonRequest(req.body_)) {
        result.jsonError(Reply::status_type::bad_request, "Invalid JSON in request");
        rep.send(result.statusCode_, "application/json");
        return;
    }

    // Here we would get the user properties here with
    // result.getSingleValue("name"), etc. and update the user

    result.buildJsonResponse([&]() -> cJSON* {
        cJSON* response = cJSON_CreateObject();
        cJSON_AddStringToObject(
            response, "message", ("User " + userId + " updated successfully").c_str());
        return response;
    });
    rep.send(Reply::status_type::ok, "application/json");
}

void MyRouterApi::usersUserIdDelete(const Request& /*req*/,
                                    Reply& rep,
                                    const std::unordered_map<std::string, std::string>& params) {
    std::string userId = params.at("userId");

    // Here we would delete the user with usersUserId

    HttpResult result(rep.content_);
    result.buildJsonResponse([&]() -> cJSON* {
        cJSON* response = cJSON_CreateObject();
        cJSON_AddStringToObject(
            response, "message", ("User " + userId + " deleted successfully").c_str());
        return response;
    });
    rep.send(Reply::status_type::ok, "application/json");
}

void MyRouterApi::usersUserIdPostsPostIdGet(
    const Request& /*req*/,
    Reply& rep,
    const std::unordered_map<std::string, std::string>& params) {
    std::string userId = params.at("userId");
    std::string postId = params.at("postId");

    HttpResult result(rep.content_);
    result.buildJsonResponse([&]() -> cJSON* {
        cJSON* post = cJSON_CreateObject();
        cJSON_AddStringToObject(post, "id", postId.c_str());
        cJSON_AddStringToObject(post, "userId", userId.c_str());
        cJSON_AddStringToObject(post, "title", ("Post " + postId + " by User " + userId).c_str());
        cJSON_AddStringToObject(post, "content", "This is the content of the post.");
        return post;
    });
    rep.send(Reply::status_type::ok, "application/json");
}

void MyRouterApi::apiInfoGet(const Request& req,
                             Reply& rep,
                             const std::unordered_map<std::string, std::string>& /*params*/) {
    HttpResult result(rep.content_);

    // Get the Origin header to demonstrate CORS handling
    std::string origin = req.getHeaderValue("Origin");

    result.buildJsonResponse([&]() -> cJSON* {
        cJSON* info = cJSON_CreateObject();
        cJSON_AddStringToObject(info, "server", "Beauty HTTP Server");
        cJSON_AddStringToObject(info, "version", "1.0.0");
        cJSON_AddStringToObject(info, "features", "REST API, CORS, HTTP/1.1");

        // Show CORS information
        cJSON* cors = cJSON_CreateObject();
        cJSON_AddBoolToObject(cors, "enabled", true);
        if (!origin.empty()) {
            cJSON_AddStringToObject(cors, "request_origin", origin.c_str());
            cJSON_AddStringToObject(cors, "status", "cross_origin_request");
        } else {
            cJSON_AddStringToObject(cors, "status", "same_origin_request");
        }
        cJSON_AddItemToObject(info, "cors", cors);

        // Show supported methods for this endpoint
        cJSON* methods = cJSON_CreateArray();
        cJSON_AddItemToArray(methods, cJSON_CreateString("GET"));
        cJSON_AddItemToArray(methods, cJSON_CreateString("HEAD"));
        cJSON_AddItemToArray(methods, cJSON_CreateString("OPTIONS"));
        cJSON_AddItemToObject(info, "supported_methods", methods);

        return info;
    });

    // Add a custom header that will be exposed via CORS
    rep.addHeader("X-Request-Id", "example-12345");
    rep.send(Reply::status_type::ok, "application/json");
}

// clang-format off
/*
 * Testing Router Functionality:
 *
 * CORS Testing:
 * 1. Same-Origin Request:
 *    curl -I http://localhost:8080/api/users
 *    (No CORS headers needed or added)
 *
 * 2. Cross-Origin Simple Request:
 *    curl -I -H "Origin: http://localhost:3000" http://localhost:8080/api/users
 *    (Should include CORS headers in response)
 *
 * 3. CORS Preflight Request:
 *     curl -I -X OPTIONS -H "Origin: http://localhost:3000" -H "Access-Control-Request-Method: POST" -H "Access-Control-Request-Headers: Content-Type" http://localhost:8080/api/users
 *     (Should return CORS preflight response)
 *
 * 4. Test CORS info endpoint:
 *    curl -I -H "Origin: http://localhost:3000" http://localhost:8080/api/info
 *    (Shows CORS status and server information)
 *
 * HTTP/1.1 Compliance Testing:
 * 5. HEAD Request (automatically supported for GET routes):
 *    curl -I http://localhost:8080/api/users
 *    (Should return headers without body)
 *
 * 6. Method Discovery (OPTIONS without CORS):
 *    curl -I -X OPTIONS http://localhost:8080/api/users
 *    (Should return Allow header with supported methods)
 *
 * 7. Unsupported Method:
 *    curl -I -X PATCH http://localhost:8080/api/users
 *    (Should return 405 Method Not Allowed with Allow header)
 */
// clang-format on
