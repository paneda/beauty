#pragma once

#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>

#include <beauty/request.hpp>
#include <beauty/reply.hpp>

namespace beauty {

enum class HandlerResult { Matched, NoMatch };

// CORS configuration structure
struct CorsConfig {
    std::unordered_set<std::string> allowedOrigins;
    std::unordered_set<std::string> allowedHeaders;
    std::unordered_set<std::string> exposedHeaders;
    bool allowCredentials = false;
    int maxAge = 86400;  // 24 hours

    // Helper methods
    bool isOriginAllowed(const std::string& origin) const;
    bool isWildcardOrigin() const;
};

// A lightweight (optional) router to be used in added RequestHandlers
class Router {
   public:
    using Handler = std::function<void(
        const Request&, Reply&, const std::unordered_map<std::string, std::string>&)>;

    // Add a route with method, path pattern and handler
    void addRoute(const std::string& method, const std::string& pathPattern, Handler handler);

    // Configure CORS settings
    void configureCors(const CorsConfig& config);

    // Handle an incoming request
    HandlerResult handle(const Request& req, Reply& rep);

   private:
    struct RouteEntry {
        std::vector<std::string> pathSegments;
        std::vector<bool> isParameter;        // true if segment is a parameter
        std::vector<std::string> paramNames;  // parameter names for segments that are parameters
        Handler handler;
    };

    // Parse a path pattern into segments and parameter flags
    RouteEntry parsePathPattern(const std::string& pathPattern, Handler handler);

    // Strip query parameters from a path (everything after '?')
    std::string stripQueryParameters(const std::string& path);

    // Split a path into segments
    std::vector<std::string> splitPath(const std::string& path);

    // Check if a request path matches a route pattern
    bool matchPath(const RouteEntry& routeEntry,
                   const std::string& requestPath,
                   std::unordered_map<std::string, std::string>& params);

    // Find all methods that support a given path
    std::vector<std::string> findAllowedMethods(const std::string& requestPath);

    // Handle CORS preflight request
    bool handleCorsPreflight(const Request& req, Reply& rep);

    // Check if a header is CORS-safelisted
    bool isCorsSafelistedHeader(const std::string& header);

    // Add CORS headers to response
    void addCorsHeaders(const Request& req, Reply& rep);

    // Check if request is a CORS preflight request
    bool isPreflightRequest(const Request& req);

    // Map of method to list of route entries
    std::unordered_map<std::string, std::vector<RouteEntry>> routes_;

    // CORS configuration
    CorsConfig corsConfig_;
    bool corsEnabled_ = false;
};

}  // namespace beauty
