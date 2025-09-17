#pragma once
#include <string>
#include <functional>
#include <unordered_map>
#include <beauty/reply.hpp>
#include <beauty/request.hpp>
#include <beauty/router.hpp>

// Example usage of the Router
// This shows how to integrate the router with Beauty web server
class MyRouterApi {
   public:
    MyRouterApi();
    virtual ~MyRouterApi() = default;

    void setupCorsConfiguration();
    void setupRoutes();
    void handleRequest(const beauty::Request& req, beauty::Reply& rep);

   private:
    // Handler implementations
    void usersGet(const beauty::Request& req,
                  beauty::Reply& rep,
                  const std::unordered_map<std::string, std::string>& params);
    void usersUserIdGet(const beauty::Request& req,
                        beauty::Reply& rep,
                        const std::unordered_map<std::string, std::string>& params);
    void usersPost(const beauty::Request& req,
                   beauty::Reply& rep,
                   const std::unordered_map<std::string, std::string>& params);
    void usersUserIdPut(const beauty::Request& req,
                        beauty::Reply& rep,
                        const std::unordered_map<std::string, std::string>& params);
    void usersUserIdDelete(const beauty::Request& req,
                           beauty::Reply& rep,
                           const std::unordered_map<std::string, std::string>& params);
    void usersUserIdPostsPostIdGet(const beauty::Request& req,
                                   beauty::Reply& rep,
                                   const std::unordered_map<std::string, std::string>& params);
    void apiInfoGet(const beauty::Request& req,
                    beauty::Reply& rep,
                    const std::unordered_map<std::string, std::string>& params);

    beauty::Router router_;
};
