#include <beauty/http_result.hpp>
#include <beauty/mime_types.hpp>

#include "my_router_api.hpp"

using namespace beauty;

// Example usage of the Router
// This shows how to integrate the router with Beauty web server
MyRouterApi::MyRouterApi() {
    setupRoutes();
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
}

// Main handler that Beauty will call
void MyRouterApi::handleRequest(const Request& req, Reply& rep) {
    if (router_.handle(req, rep) == HandlerResult::Matched) {
        return;  // Request was handled by router
    } else if (router_.handle(req, rep) == HandlerResult::MethodNotSupported) {
        // Method not supported for this path.
        // Note: HTTP 1.1 requires 405 "Method Not Allowed" to include the
        // "Allow" header
        rep.addHeader("Allow", router_.getAllowedMethodsWhenMethodNotSupported());
        rep.send(Reply::status_type::method_not_allowed);
        return;
    }

    // If no route matched let another handler or finally FileIO match the route.
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
