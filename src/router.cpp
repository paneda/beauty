#include <cctype>
#include <set>
#include <sstream>
#include <algorithm>

#include "beauty/router.hpp"

namespace beauty {

// CorsConfig helper methods
bool CorsConfig::isOriginAllowed(const std::string& origin) const {
    if (isWildcardOrigin())
        return true;
    return allowedOrigins.find(origin) != allowedOrigins.end();
}

bool CorsConfig::isWildcardOrigin() const {
    return allowedOrigins.find("*") != allowedOrigins.end();
}

void Router::configureCors(const CorsConfig& config) {
    corsConfig_ = config;
    corsEnabled_ = true;
}

void Router::addRoute(const std::string& method, const std::string& pathPattern, Handler handler) {
    RouteEntry entry = parsePathPattern(pathPattern, handler);
    auto& vec = routes_[method];
    vec.push_back(std::move(entry));
    // Sort: more literal segments (fewer parameters) first
    std::sort(vec.begin(), vec.end(), [](const RouteEntry& a, const RouteEntry& b) {
        int aParams = std::count(a.isParameter.begin(), a.isParameter.end(), true);
        int bParams = std::count(b.isParameter.begin(), b.isParameter.end(), true);
        return aParams < bParams;
    });

    // If this is a GET route, also add a HEAD route with the same handler
    if (method == "GET") {
        RouteEntry headEntry = parsePathPattern(pathPattern, handler);
        auto& headVec = routes_["HEAD"];
        headVec.push_back(std::move(headEntry));
        // Sort HEAD routes the same way
        std::sort(headVec.begin(), headVec.end(), [](const RouteEntry& a, const RouteEntry& b) {
            int aParams = std::count(a.isParameter.begin(), a.isParameter.end(), true);
            int bParams = std::count(b.isParameter.begin(), b.isParameter.end(), true);
            return aParams < bParams;
        });
    }
}

HandlerResult Router::handle(const Request& req, Reply& rep) {
    auto methodIt = routes_.find(req.method_);

    // Handle CORS preflight requests first
    if (corsEnabled_ && isPreflightRequest(req)) {
        return handleCorsPreflight(req, rep) ? HandlerResult::Matched : HandlerResult::NoMatch;
    }

    // Handle OPTIONS method specially
    if (req.method_ == "OPTIONS") {
        std::vector<std::string> allowedMethods = findAllowedMethods(req.requestPath_);

        if (!allowedMethods.empty()) {
            std::ostringstream oss;
            for (size_t i = 0; i < allowedMethods.size(); ++i) {
                if (i > 0)
                    oss << ", ";
                oss << allowedMethods[i];
            }

            rep.addHeader("Allow", oss.str());
            if (corsEnabled_) {
                addCorsHeaders(req, rep);
            }
            rep.send(Reply::ok);
            return HandlerResult::Matched;
        } else {
            // Path not found, return 404
            return HandlerResult::NoMatch;
        }
    }

    // Try to match path+method first
    if (methodIt != routes_.end()) {
        for (const auto& routeEntry : methodIt->second) {
            std::unordered_map<std::string, std::string> params;
            if (matchPath(routeEntry, req.requestPath_, params)) {
                routeEntry.handler(req, rep, params);
                // Add CORS headers to successful responses
                if (corsEnabled_) {
                    addCorsHeaders(req, rep);
                }
                return HandlerResult::Matched;
            }
        }
    }

    // If not matched, check if path exists for any other method and collect allowed methods
    if (req.httpVersionMajor_ == 1 && req.httpVersionMinor_ == 1) {
        std::vector<std::string> allowedMethods = findAllowedMethods(req.requestPath_);

        // Remove the current method from allowed methods (since it didn't match)
        allowedMethods.erase(std::remove(allowedMethods.begin(), allowedMethods.end(), req.method_),
                             allowedMethods.end());

        if (!allowedMethods.empty()) {
            std::ostringstream oss;
            for (size_t i = 0; i < allowedMethods.size(); ++i) {
                if (i > 0)
                    oss << ", ";
                oss << allowedMethods[i];
            }

            rep.addHeader("Allow", oss.str());
            rep.addHeader("Connection", "close");
            rep.send(Reply::method_not_allowed);
            return HandlerResult::NoMatch;
        }
    }

    // No path matched at all
    return HandlerResult::NoMatch;
}

std::vector<std::string> Router::findAllowedMethods(const std::string& requestPath) {
    std::vector<std::string> allowedMethods;

    for (const auto& it : routes_) {
        const std::string& method = it.first;
        const std::vector<RouteEntry>& entries = it.second;
        for (const auto& routeEntry : entries) {
            std::unordered_map<std::string, std::string> params;
            if (matchPath(routeEntry, requestPath, params)) {
                allowedMethods.push_back(method);
                break;  // Only need to add each method once
            }
        }
    }

    return allowedMethods;
}

Router::RouteEntry Router::parsePathPattern(const std::string& pathPattern, Handler handler) {
    RouteEntry entry;
    entry.handler = handler;

    std::vector<std::string> segments = splitPath(pathPattern);

    for (const std::string& segment : segments) {
        if (segment.size() >= 2 && segment[0] == '{' && segment[segment.size() - 1] == '}') {
            // This is a parameter
            std::string paramName = segment.substr(1, segment.size() - 2);
            entry.pathSegments.push_back(paramName);
            entry.isParameter.push_back(true);
            entry.paramNames.push_back(paramName);
        } else {
            // This is a literal segment
            entry.pathSegments.push_back(segment);
            entry.isParameter.push_back(false);
            entry.paramNames.push_back("");  // Empty string for non-parameters
        }
    }

    return entry;
}

std::string Router::stripQueryParameters(const std::string& path) {
    // Find the first '?' character and return everything before it
    size_t queryPos = path.find('?');
    if (queryPos != std::string::npos) {
        return path.substr(0, queryPos);
    }
    return path;
}

std::vector<std::string> Router::splitPath(const std::string& path) {
    std::vector<std::string> segments;

    // First, strip query parameters from the path
    std::string pathWithoutQuery = stripQueryParameters(path);

    if (pathWithoutQuery.empty() || pathWithoutQuery == "/") {
        return segments;
    }

    std::string cleanPath = pathWithoutQuery;
    // Remove leading slash if present
    if (cleanPath[0] == '/') {
        cleanPath = cleanPath.substr(1);
    }

    if (cleanPath.empty()) {
        return segments;
    }

    std::stringstream ss(cleanPath);
    std::string segment;

    while (std::getline(ss, segment, '/')) {
        if (!segment.empty()) {
            segments.push_back(segment);
        }
    }

    return segments;
}

bool Router::matchPath(const RouteEntry& routeEntry,
                       const std::string& requestPath,
                       std::unordered_map<std::string, std::string>& params) {
    std::vector<std::string> requestSegments = splitPath(requestPath);

    // Check if segment counts match
    if (requestSegments.size() != routeEntry.pathSegments.size()) {
        return false;
    }

    // Check each segment
    for (size_t i = 0; i < routeEntry.pathSegments.size(); ++i) {
        if (routeEntry.isParameter[i]) {
            // This is a parameter, extract it
            params[routeEntry.paramNames[i]] = requestSegments[i];
        } else {
            // This is a literal segment, must match exactly
            if (routeEntry.pathSegments[i] != requestSegments[i]) {
                return false;
            }
        }
    }

    return true;
}

bool Router::isPreflightRequest(const Request& req) {
    if (req.method_ != "OPTIONS")
        return false;

    // Check for required CORS preflight headers
    std::string origin = req.getHeaderValue("Origin");
    std::string requestMethod = req.getHeaderValue("Access-Control-Request-Method");

    return !origin.empty() && !requestMethod.empty();
}

bool Router::handleCorsPreflight(const Request& req, Reply& rep) {
    std::string origin = req.getHeaderValue("Origin");
    std::string requestMethod = req.getHeaderValue("Access-Control-Request-Method");
    std::string requestHeaders = req.getHeaderValue("Access-Control-Request-Headers");

    // Check if origin is allowed
    if (!corsConfig_.isOriginAllowed(origin)) {
        return false;  // Forbidden
    }

    // Check if the requested method is supported for this path
    std::vector<std::string> allowedMethods = findAllowedMethods(req.requestPath_);
    if (std::find(allowedMethods.begin(), allowedMethods.end(), requestMethod) ==
        allowedMethods.end()) {
        return false;  // Method not allowed for this path
    }

    // Set CORS preflight response headers
    rep.addHeader("Access-Control-Allow-Origin", corsConfig_.isWildcardOrigin() ? "*" : origin);

    // Allow the requested method plus any other methods for this path
    std::ostringstream methodsOss;
    for (size_t i = 0; i < allowedMethods.size(); ++i) {
        if (i > 0)
            methodsOss << ", ";
        methodsOss << allowedMethods[i];
    }
    rep.addHeader("Access-Control-Allow-Methods", methodsOss.str());

    // Handle requested headers
    if (!requestHeaders.empty()) {
        std::vector<std::string> nonSafelistedHeaders;

        // Parse the requested headers
        std::istringstream iss(requestHeaders);
        std::string header;
        while (std::getline(iss, header, ',')) {
            // Remove leading/trailing whitespace
            header.erase(0, header.find_first_not_of(" \t"));
            header.erase(header.find_last_not_of(" \t") + 1);

            // Skip CORS-safelisted headers (they don't need explicit permission)
            if (isCorsSafelistedHeader(header)) {
                continue;
            }

            // For non-safelisted headers, check if they're in our allowed list
            if (corsConfig_.allowedHeaders.find(header) != corsConfig_.allowedHeaders.end()) {
                nonSafelistedHeaders.push_back(header);
            }
        }

        // Only set the header if we have non-safelisted headers to return
        if (!nonSafelistedHeaders.empty()) {
            std::ostringstream headersOss;
            for (size_t i = 0; i < nonSafelistedHeaders.size(); ++i) {
                if (i > 0)
                    headersOss << ", ";
                headersOss << nonSafelistedHeaders[i];
            }
            rep.addHeader("Access-Control-Allow-Headers", headersOss.str());
        }
    }

    // Set max age
    rep.addHeader("Access-Control-Max-Age", std::to_string(corsConfig_.maxAge));

    if (corsConfig_.allowCredentials && !corsConfig_.isWildcardOrigin()) {
        rep.addHeader("Access-Control-Allow-Credentials", "true");
    }

    rep.send(Reply::ok);
    return true;
}

bool Router::isCorsSafelistedHeader(const std::string& header) {
    // Convert to lowercase for case-insensitive comparison
    std::string lowerHeader = header;
    std::transform(lowerHeader.begin(), lowerHeader.end(), lowerHeader.begin(), ::tolower);

    // CORS-safelisted request headers (don't need explicit permission)
    static const std::set<std::string> safelistedHeaders = {
        "accept",
        "accept-language",
        "content-language",
        "content-type"  // Note: Only certain values are safelisted, but we'll be permissive here
    };

    return safelistedHeaders.find(lowerHeader) != safelistedHeaders.end();
}

void Router::addCorsHeaders(const Request& req, Reply& rep) {
    std::string origin = req.getHeaderValue("Origin");

    if (origin.empty())
        return;  // Not a cross-origin request

    if (!corsConfig_.isOriginAllowed(origin))
        return;  // Origin not allowed

    // Add basic CORS headers
    rep.addHeader("Access-Control-Allow-Origin", corsConfig_.isWildcardOrigin() ? "*" : origin);

    if (corsConfig_.allowCredentials && !corsConfig_.isWildcardOrigin()) {
        rep.addHeader("Access-Control-Allow-Credentials", "true");
    }

    // Add exposed headers if any
    if (!corsConfig_.exposedHeaders.empty()) {
        std::ostringstream oss;
        bool first = true;
        for (const auto& header : corsConfig_.exposedHeaders) {
            if (!first)
                oss << ", ";
            oss << header;
            first = false;
        }
        rep.addHeader("Access-Control-Expose-Headers", oss.str());
    }
}

}  // namespace beauty
