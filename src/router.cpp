#include <sstream>

#include "beauty/router.hpp"

namespace beauty {

void Router::addRoute(const std::string& method, const std::string& pathPattern, Handler handler) {
    RouteEntry entry = parsePathPattern(pathPattern, handler);
    routes_[method].push_back(std::move(entry));
}

HandlerResult  Router::handle(const Request& req, Reply& rep) {
	auto methodIt = routes_.find(req.method_);
	std::map<std::string, std::string> params;

	// Try to match path+method first
	if (methodIt != routes_.end()) {
		for (const auto& routeEntry : methodIt->second) {
			if (matchPath(routeEntry, req.requestPath_, params)) {
				routeEntry.handler(req, rep, params);
				return HandlerResult::Matched;
			}
			params.clear();
		}
	}

	// If not matched, check if path exists for any other method
	for (auto it = routes_.begin(); it != routes_.end(); ++it) {
		const std::string& otherMethod = it->first;
		const std::vector<RouteEntry>& entries = it->second;
		if (otherMethod == req.method_) {
			continue;
		}
		for (const auto& routeEntry : entries) {
			if (matchPath(routeEntry, req.requestPath_, params)) {
				return HandlerResult::MethodNotSupported;
			}
			params.clear();
		}
	}

	// No path matched at all
	return HandlerResult::NoMatch;
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
            entry.paramNames.push_back(""); // Empty string for non-parameters
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
    // Remove trailing slash if present
    if (!cleanPath.empty() && cleanPath[cleanPath.size() - 1] == '/') {
        cleanPath = cleanPath.substr(0, cleanPath.size() - 1);
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

bool Router::matchPath(const RouteEntry& routeEntry, const std::string& requestPath, 
                                  std::map<std::string, std::string>& params) {
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

} // namespace beauty
