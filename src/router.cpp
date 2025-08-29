#include <sstream>

#include "beauty/router.hpp"

namespace beauty {

void Router::addRoute(const std::string& method, const std::string& pathPattern, Handler handler) {
    // RouteEntry entry = parsePathPattern(pathPattern, handler);
    // routes_[method].push_back(std::move(entry));
    RouteEntry entry = parsePathPattern(pathPattern, handler);
    auto& vec = routes_[method];
    vec.push_back(std::move(entry));
    // Sort: more literal segments (fewer parameters) first
    std::sort(vec.begin(), vec.end(), [](const RouteEntry& a, const RouteEntry& b) {
        int aParams = std::count(a.isParameter.begin(), a.isParameter.end(), true);
        int bParams = std::count(b.isParameter.begin(), b.isParameter.end(), true);
        return aParams < bParams;
    });
}

HandlerResult Router::handle(const Request& req, Reply& rep) {
    auto methodIt = routes_.find(req.method_);

    // Try to match path+method first
    if (methodIt != routes_.end()) {
        for (const auto& routeEntry : methodIt->second) {
            std::unordered_map<std::string, std::string> params;
            if (matchPath(routeEntry, req.requestPath_, params)) {
                routeEntry.handler(req, rep, params);
                return HandlerResult::Matched;
            }
        }
    }

    // If not matched, check if path exists for any other method and collect allowed methods
    if (req.httpVersionMajor_ == 1 && req.httpVersionMinor_ == 1) {
        bool pathMatched = false;
        std::ostringstream oss;
        bool first = true;
        for (const auto& it : routes_) {
            const std::string& otherMethod = it.first;
            if (otherMethod == req.method_) {
                continue;
            }
            const std::vector<RouteEntry>& entries = it.second;
            for (const auto& routeEntry : entries) {
                std::unordered_map<std::string, std::string> params;
                if (matchPath(routeEntry, req.requestPath_, params)) {
                    if (!first) {
                        oss << ", ";
                    }
                    oss << otherMethod;
                    first = false;
                    pathMatched = true;
                    break;  // Only need to add each method once
                }
            }
        }

        if (pathMatched) {
            allowedMethodsWhenMethodNotSupported_ = oss.str();
            return HandlerResult::MethodNotSupported;
        }
    }

    // No path matched at all
    allowedMethodsWhenMethodNotSupported_.clear();
    return HandlerResult::NoMatch;
}

const std::string& Router::getAllowedMethodsWhenMethodNotSupported() const {
    return allowedMethodsWhenMethodNotSupported_;
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

std::vector<std::string> Router::splitPath(const std::string& path) {
    std::vector<std::string> segments;

    if (path.empty() || path == "/") {
        return segments;
    }

    std::string cleanPath = path;
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

}  // namespace beauty
