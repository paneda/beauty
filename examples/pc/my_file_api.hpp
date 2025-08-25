#pragma once

#include <string>
#include <beauty/reply.hpp>
#include <beauty/request.hpp>

class MyFileApi {
   public:
    MyFileApi(const std::string &docRoot);
    virtual ~MyFileApi() = default;

    void handleRequest(const beauty::Request &req, beauty::Reply &rep);

   private:
    const std::string docRoot_;
};
