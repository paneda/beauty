#include "lightweight_router.hpp"
#include <beauty/http_result.hpp>
#include <iostream>

namespace beauty {

/**
 * Example usage of the Router
 * This shows how to integrate the router with Beauty web server
 */
class RouterExample {
private:
    Router router_;

public:
    RouterExample() {
        setupRoutes();
    }

    void setupRoutes() {
        // Route: GET /users
        router_.addRoute("GET", "/users", 
            [this](const Request& req, Reply& rep, const std::map<std::string, std::string>& params) {
                this->usersGet(req, rep, params);
            });

        // Route: GET /users/{userId}
        router_.addRoute("GET", "/users/{userId}", 
            [this](const Request& req, Reply& rep, const std::map<std::string, std::string>& params) {
                this->usersUserIdGet(req, rep, params);
            });

        // Route: POST /users
        router_.addRoute("POST", "/users", 
            [this](const Request& req, Reply& rep, const std::map<std::string, std::string>& params) {
                this->usersPost(req, rep, params);
            });

        // Route: PUT /users/{userId}
        router_.addRoute("PUT", "/users/{userId}", 
            [this](const Request& req, Reply& rep, const std::map<std::string, std::string>& params) {
                this->usersUserIdPut(req, rep, params);
            });

        // Route: DELETE /users/{userId}
        router_.addRoute("DELETE", "/users/{userId}", 
            [this](const Request& req, Reply& rep, const std::map<std::string, std::string>& params) {
                this->usersUserIdDelete(req, rep, params);
            });

        // Route: GET /users/{userId}/posts/{postId}
        router_.addRoute("GET", "/users/{userId}/posts/{postId}", 
            [this](const Request& req, Reply& rep, const std::map<std::string, std::string>& params) {
                this->usersUserIdPostsPostIdGet(req, rep, params);
            });
    }

    // Main handler that Beauty will call
    void handleRequest(const Request& req, Reply& rep) {
        if (router_.handle(req, rep)) {
            return; // Request was handled by router
        }
        
        // If no route matched, return 404
        HttpResult result(rep.content_);
        result.jsonError(Reply::status_type::not_found, "Route not found");
        rep.send(result.statusCode_, "application/json");
    }

private:
    // Handler implementations
    void usersGet(const Request& req, Reply& rep, const std::map<std::string, std::string>& params) {
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
        rep.send(result.statusCode_, "application/json");
    }

    void usersUserIdGet(const Request& req, Reply& rep, const std::map<std::string, std::string>& params) {
        std::string userId = params.at("userId");
        
        HttpResult result(rep.content_);
        result.buildJsonResponse([&]() -> cJSON* {
            cJSON* user = cJSON_CreateObject();
            cJSON_AddStringToObject(user, "id", userId.c_str());
            cJSON_AddStringToObject(user, "name", ("User " + userId).c_str());
            cJSON_AddStringToObject(user, "email", ("user" + userId + "@example.com").c_str());
            return user;
        });
        rep.send(result.statusCode_, "application/json");
    }

    void usersPost(const Request& req, Reply& rep, const std::map<std::string, std::string>& params) {
        HttpResult result(rep.content_);
        result.buildJsonResponse([&]() -> cJSON* {
            cJSON* response = cJSON_CreateObject();
            cJSON_AddStringToObject(response, "message", "User created successfully");
            cJSON_AddStringToObject(response, "id", "123");
            return response;
        });
        rep.send(Reply::status_type::created, "application/json");
    }

    void usersUserIdPut(const Request& req, Reply& rep, const std::map<std::string, std::string>& params) {
        std::string userId = params.at("userId");
        
        HttpResult result(rep.content_);
        result.buildJsonResponse([&]() -> cJSON* {
            cJSON* response = cJSON_CreateObject();
            cJSON_AddStringToObject(response, "message", ("User " + userId + " updated successfully").c_str());
            return response;
        });
        rep.send(result.statusCode_, "application/json");
    }

    void usersUserIdDelete(const Request& req, Reply& rep, const std::map<std::string, std::string>& params) {
        std::string userId = params.at("userId");
        
        HttpResult result(rep.content_);
        result.buildJsonResponse([&]() -> cJSON* {
            cJSON* response = cJSON_CreateObject();
            cJSON_AddStringToObject(response, "message", ("User " + userId + " deleted successfully").c_str());
            return response;
        });
        rep.send(result.statusCode_, "application/json");
    }

    void usersUserIdPostsPostIdGet(const Request& req, Reply& rep, const std::map<std::string, std::string>& params) {
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
        rep.send(result.statusCode_, "application/json");
    }
};

} // namespace beauty
