#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include <beauty/request.hpp>
#include <beauty/reply.hpp>

namespace beauty {

enum class HandlerResult {Matched, MethodNotSupported, NoMatch};

// Lightweight (optional) router
class Router {
   public:
    using Handler =
        std::function<void(const Request&, Reply&, const std::map<std::string, std::string>&)>;

    // Add a route with method, path pattern and handler
    void addRoute(const std::string& method, const std::string& pathPattern, Handler handler);

    // Handle an incoming request
    HandlerResult handle(const Request& req, Reply& rep);

   private:
    struct RouteEntry {
        std::vector<std::string> pathSegments;
        std::vector<bool> isParameter;        // true if segment is a parameter
        std::vector<std::string> paramNames;  // parameter names for segments that are parameters
        Handler handler;
    };

    std::map<std::string, std::vector<RouteEntry>> routes_;

    // Parse a path pattern into segments and parameter flags
    RouteEntry parsePathPattern(const std::string& pathPattern, Handler handler);

    // Split a path into segments
    std::vector<std::string> splitPath(const std::string& path);

    // Check if a request path matches a route pattern
    bool matchPath(const RouteEntry& routeEntry,
                   const std::string& requestPath,
                   std::map<std::string, std::string>& params);
};

}  // namespace beauty
