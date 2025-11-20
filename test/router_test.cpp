#include <set>
#include <sstream>
#include <cassert>
#include <catch2/catch_test_macros.hpp>
#include "beauty/router.hpp"

using namespace beauty;

namespace {
// Helper to split comma-separated methods
std::set<std::string> split_methods(const std::string& allow_header) {
    std::set<std::string> methods;
    std::istringstream iss(allow_header);
    std::string method;
    while (std::getline(iss, method, ',')) {
        // Remove leading/trailing whitespace
        method.erase(0, method.find_first_not_of(" \t"));
        method.erase(method.find_last_not_of(" \t") + 1);
        methods.insert(method);
    }
    return methods;
}
}  // namespace

TEST_CASE("router functionality", "[router]") {
    Router router;
    bool handlerCalled = false;
    std::unordered_map<std::string, std::string> capturedParams;

    SECTION("should match route with parameter and extract param") {
        // Add a test route
        router.addRoute("GET",
                        "/users/{userId}",
                        [&](const Request&,
                            Reply&,
                            const std::unordered_map<std::string, std::string>& params) {
                            handlerCalled = true;
                            capturedParams = params;
                        });

        // Create a mock request
        std::vector<char> body;
        Request req(body);
        req.method_ = "GET";
        req.requestPath_ = "/users/123";

        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);

        HandlerResult handled = router.handle(req, rep);

        REQUIRE(handled == HandlerResult::Matched);
        REQUIRE(handlerCalled == true);
        REQUIRE(capturedParams.size() == 1);
        REQUIRE(capturedParams["userId"] == "123");
    }
    SECTION(
        "should set Allow header for HTTP 1.1 request and existing path with different method") {
        // Add a test route
        router.addRoute(
            "GET",
            "/users/{userId}",
            [&](const Request&, Reply&, const std::unordered_map<std::string, std::string>&) {});
        router.addRoute(
            "POST",
            "/users/{userId}",
            [&](const Request&, Reply&, const std::unordered_map<std::string, std::string>&) {});
        router.addRoute(
            "DELETE",
            "/users/{userId}",
            [&](const Request&, Reply&, const std::unordered_map<std::string, std::string>&) {});

        // Create a mock request with different method
        std::vector<char> body;
        Request req(body);
        req.method_ = "PUT";
        req.requestPath_ = "/users/123";
        req.httpVersionMajor_ = 1;
        req.httpVersionMinor_ = 1;

        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);

        HandlerResult handled = router.handle(req, rep);

        REQUIRE(handled == HandlerResult::NoMatch);
        REQUIRE(handlerCalled == false);
        auto allowedMethods = rep.getHeaderValue("Allow");
        // Test allowed methods regardless of order
        REQUIRE(split_methods(allowedMethods) ==
                std::set<std::string>({"GET", "HEAD", "POST", "DELETE"}));
        REQUIRE(rep.getStatus() == Reply::method_not_allowed);
    }
    SECTION("should return NoMatch for HTTP 1.0 request and existing path with different method") {
        // Add a test route
        router.addRoute(
            "GET",
            "/users/{userId}",
            [&](const Request&, Reply&, const std::unordered_map<std::string, std::string>&) {});
        router.addRoute(
            "POST",
            "/users/{userId}",
            [&](const Request&, Reply&, const std::unordered_map<std::string, std::string>&) {});
        router.addRoute(
            "DELETE",
            "/users/{userId}",
            [&](const Request&, Reply&, const std::unordered_map<std::string, std::string>&) {});

        // Create a mock request with different method
        std::vector<char> body;
        Request req(body);
        req.method_ = "PUT";
        req.requestPath_ = "/users/123";
        req.httpVersionMajor_ = 1;
        req.httpVersionMinor_ = 0;

        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);

        HandlerResult handled = router.handle(req, rep);

        REQUIRE(handled == HandlerResult::NoMatch);
        REQUIRE(handlerCalled == false);
    }
    SECTION("should return NoMatch for non-existing path") {
        // Add a test route
        router.addRoute(
            "GET",
            "/users/{userId}",
            [&](const Request&, Reply&, const std::unordered_map<std::string, std::string>&) {});
        // Create a mock request with non-matching path
        std::vector<char> body;
        Request req(body);
        req.method_ = "GET";
        req.requestPath_ = "/unknown/path";

        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);

        HandlerResult handled = router.handle(req, rep);

        REQUIRE(handled == HandlerResult::NoMatch);
        REQUIRE(handlerCalled == false);
    }
    SECTION("should match route without parameters") {
        // Add a test route
        router.addRoute(
            "GET",
            "/status",
            [&](const Request&, Reply&, const std::unordered_map<std::string, std::string>&) {
                handlerCalled = true;
            });

        // Create a mock request
        std::vector<char> body;
        Request req(body);
        req.method_ = "GET";
        req.requestPath_ = "/status";

        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);

        HandlerResult handled = router.handle(req, rep);

        REQUIRE(handled == HandlerResult::Matched);
        REQUIRE(handlerCalled == true);
    }
    SECTION("should not match partial paths") {
        // Add a test route
        router.addRoute(
            "GET",
            "/users/{userId}/posts/{postId}",
            [&](const Request&, Reply&, const std::unordered_map<std::string, std::string>&) {
                handlerCalled = true;
            });

        // Create a mock request with partial path
        std::vector<char> body;
        Request req(body);
        req.method_ = "GET";
        req.requestPath_ = "/users/123/posts";  // Missing postId

        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);

        HandlerResult handled = router.handle(req, rep);

        REQUIRE(handled == HandlerResult::NoMatch);
        REQUIRE(handlerCalled == false);
    }
    SECTION("should match complex paths with multiple parameters") {
        // Add a test route
        router.addRoute("GET",
                        "/users/{userId}/posts/{postId}",
                        [&](const Request&,
                            Reply&,
                            const std::unordered_map<std::string, std::string>& params) {
                            handlerCalled = true;
                            capturedParams = params;
                        });

        // Create a mock request
        std::vector<char> body;
        Request req(body);
        req.method_ = "GET";
        req.requestPath_ = "/users/456/posts/789";

        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);

        HandlerResult handled = router.handle(req, rep);

        REQUIRE(handled == HandlerResult::Matched);
        REQUIRE(handlerCalled == true);
        REQUIRE(capturedParams.size() == 2);
        REQUIRE(capturedParams["userId"] == "456");
        REQUIRE(capturedParams["postId"] == "789");
    }
    SECTION("should handle empty path") {
        // Add a test route for empty path
        router.addRoute(
            "GET",
            "",
            [&](const Request&, Reply&, const std::unordered_map<std::string, std::string>&) {
                handlerCalled = true;
            });

        // Create a mock request with empty path
        std::vector<char> body;
        Request req(body);
        req.method_ = "GET";
        req.requestPath_ = "";

        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);

        HandlerResult handled = router.handle(req, rep);

        REQUIRE(handled == HandlerResult::Matched);
        REQUIRE(handlerCalled == true);
    }
    SECTION("should handle root path") {
        // Add a test route
        router.addRoute(
            "GET",
            "/",
            [&](const Request&, Reply&, const std::unordered_map<std::string, std::string>&) {
                handlerCalled = true;
            });

        // Create a mock request
        std::vector<char> body;
        Request req(body);
        req.method_ = "GET";
        req.requestPath_ = "/";

        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);

        HandlerResult handled = router.handle(req, rep);

        REQUIRE(handled == HandlerResult::Matched);
        REQUIRE(handlerCalled == true);
    }
    SECTION("should ignore trailing slashes in path matching") {
        // Add a test route without trailing slash
        router.addRoute(
            "GET",
            "/api/resource",
            [&](const Request&, Reply&, const std::unordered_map<std::string, std::string>&) {
                handlerCalled = true;
            });

        // Create a mock request with trailing slash
        std::vector<char> body;
        Request req(body);
        req.method_ = "GET";
        req.requestPath_ = "/api/resource/";

        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);

        HandlerResult handled = router.handle(req, rep);

        REQUIRE(handled == HandlerResult::Matched);
        REQUIRE(handlerCalled == true);
    }
    SECTION("should ignore trailing slashes in route definition") {
        // Add a test route without trailing slash
        router.addRoute(
            "GET",
            "/api/resource/",
            [&](const Request&, Reply&, const std::unordered_map<std::string, std::string>&) {
                handlerCalled = true;
            });

        // Create a mock request without the trailing slash
        std::vector<char> body;
        Request req(body);
        req.method_ = "GET";
        req.requestPath_ = "/api/resource";

        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);

        HandlerResult handled = router.handle(req, rep);

        REQUIRE(handled == HandlerResult::Matched);
        REQUIRE(handlerCalled == true);
    }
    SECTION("should handle multiple routes and match correct one") {
        // Add multiple test routes
        router.addRoute(
            "POST",
            "/items",
            [&](const Request&, Reply&, const std::unordered_map<std::string, std::string>&) {
                handlerCalled = true;
            });
        router.addRoute("GET",
                        "/items/{itemId}",
                        [&](const Request&,
                            Reply&,
                            const std::unordered_map<std::string, std::string>& params) {
                            handlerCalled = true;
                            capturedParams = params;
                        });
        router.addRoute(
            "GET",
            "/status",
            [&](const Request&, Reply&, const std::unordered_map<std::string, std::string>&) {
                handlerCalled = true;
            });

        // Create a mock request that should match the second route
        std::vector<char> body;
        Request req(body);
        req.method_ = "GET";
        req.requestPath_ = "/items/42";

        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);

        HandlerResult handled = router.handle(req, rep);

        REQUIRE(handled == HandlerResult::Matched);
        REQUIRE(handlerCalled == true);
        REQUIRE(capturedParams.size() == 1);
        REQUIRE(capturedParams["itemId"] == "42");
    }
    SECTION("should handle overlapping routes and match most specific one") {
        // Add overlapping test routes
        router.addRoute("GET",
                        "/files/{fileId}",
                        [&](const Request&,
                            Reply&,
                            const std::unordered_map<std::string, std::string>& params) {
                            handlerCalled = true;
                            capturedParams = params;
                        });
        router.addRoute(
            "GET",
            "/files/special",
            [&](const Request&, Reply&, const std::unordered_map<std::string, std::string>&) {
                handlerCalled = true;
                capturedParams.clear();
                capturedParams["special"] = "true";
            });

        // Create a mock request that should match the more specific route
        std::vector<char> body;
        Request req(body);
        req.method_ = "GET";
        req.requestPath_ = "/files/special";

        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);

        HandlerResult handled = router.handle(req, rep);

        REQUIRE(handled == HandlerResult::Matched);
        REQUIRE(handlerCalled == true);
        REQUIRE(capturedParams.size() == 1);
        REQUIRE(capturedParams["special"] == "true");
    }
    SECTION("should handle routes with similar prefixes") {
        // Add routes with similar prefixes
        router.addRoute(
            "GET",
            "/api/v1/resource",
            [&](const Request&, Reply&, const std::unordered_map<std::string, std::string>&) {
                handlerCalled = true;
            });
        router.addRoute("GET",
                        "/api/v1/resource/{id}",
                        [&](const Request&,
                            Reply&,
                            const std::unordered_map<std::string, std::string>& params) {
                            handlerCalled = true;
                            capturedParams = params;
                        });
        // Create a mock request that should match the second route
        std::vector<char> body;
        Request req(body);
        req.method_ = "GET";
        req.requestPath_ = "/api/v1/resource/99";
        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);
        HandlerResult handled = router.handle(req, rep);
        REQUIRE(handled == HandlerResult::Matched);
        REQUIRE(handlerCalled == true);
        REQUIRE(capturedParams.size() == 1);
        REQUIRE(capturedParams["id"] == "99");
    }
    SECTION("should handle OPTIONS request for existing path") {
        // Add a test route
        router.addRoute(
            "GET",
            "/options/test",
            [&](const Request&, Reply&, const std::unordered_map<std::string, std::string>&) {
                handlerCalled = true;
            });
        router.addRoute(
            "POST",
            "/options/test",
            [&](const Request&, Reply&, const std::unordered_map<std::string, std::string>&) {
                handlerCalled = true;
            });
        // Create a mock OPTIONS request
        std::vector<char> body;
        Request req(body);
        req.method_ = "OPTIONS";
        req.requestPath_ = "/options/test";
        req.httpVersionMajor_ = 1;
        req.httpVersionMinor_ = 1;
        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);
        HandlerResult handled = router.handle(req, rep);
        REQUIRE(handled == HandlerResult::Matched);
        REQUIRE(handlerCalled == false);  // Handler should not be called for OPTIONS
        auto allowedMethods = rep.getHeaderValue("Allow");
        REQUIRE(split_methods(allowedMethods) == std::set<std::string>({"GET", "HEAD", "POST"}));
        REQUIRE(rep.getStatus() == Reply::ok);
    }
    SECTION("should handle paths with query parameters") {
        // Add a test route
        router.addRoute(
            "GET",
            "/search",
            [&](const Request&, Reply&, const std::unordered_map<std::string, std::string>&) {
                handlerCalled = true;
            });
        // Create a mock request with query parameters
        std::vector<char> body;
        Request req(body);
        req.method_ = "GET";
        req.requestPath_ = "/search?q=test&sort=asc";
        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);
        HandlerResult handled = router.handle(req, rep);
        REQUIRE(handled == HandlerResult::Matched);
        REQUIRE(handlerCalled == true);
    }
}

TEST_CASE("router CORS functionality", "[router][cors]") {
    Router router;
    bool handlerCalled = false;

    // Add a test route
    router.addRoute(
        "GET",
        "/api/users",
        [&](const Request&, Reply&, const std::unordered_map<std::string, std::string>&) {
            handlerCalled = true;
        });
    router.addRoute(
        "POST",
        "/api/users",
        [&](const Request&, Reply&, const std::unordered_map<std::string, std::string>&) {
            handlerCalled = true;
        });

    SECTION("should handle requests without CORS configuration") {
        std::vector<char> body;
        Request req(body);
        req.method_ = "GET";
        req.requestPath_ = "/api/users";
        req.headers_.push_back({"Origin", "https://example.com"});

        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);
        HandlerResult handled = router.handle(req, rep);

        REQUIRE(handled == HandlerResult::Matched);
        REQUIRE(handlerCalled == true);
        // No CORS headers should be added when CORS is not configured
        REQUIRE(rep.getHeaderValue("Access-Control-Allow-Origin").empty());
    }

    SECTION("should configure CORS and add headers for cross-origin requests") {
        // Configure CORS
        CorsConfig corsConfig;
        corsConfig.allowedOrigins.insert("https://example.com");
        corsConfig.allowedOrigins.insert("https://app.example.com");
        corsConfig.allowedHeaders.insert("Content-Type");
        corsConfig.allowedHeaders.insert("Authorization");
        corsConfig.exposedHeaders.insert("X-Total-Count");
        corsConfig.allowCredentials = true;
        corsConfig.maxAge = 3600;
        router.configureCors(corsConfig);

        std::vector<char> body;
        Request req(body);
        req.method_ = "GET";
        req.requestPath_ = "/api/users";
        req.headers_.push_back({"Origin", "https://example.com"});

        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);
        HandlerResult handled = router.handle(req, rep);

        REQUIRE(handled == HandlerResult::Matched);
        REQUIRE(handlerCalled == true);
        REQUIRE(rep.getHeaderValue("Access-Control-Allow-Origin") == "https://example.com");
        REQUIRE(rep.getHeaderValue("Access-Control-Allow-Credentials") == "true");
        REQUIRE(rep.getHeaderValue("Access-Control-Expose-Headers") == "X-Total-Count");
    }

    SECTION("should handle wildcard origin") {
        // Configure CORS with wildcard
        CorsConfig corsConfig;
        corsConfig.allowedOrigins.insert("*");
        corsConfig.allowCredentials = false;  // Cannot use credentials with wildcard
        router.configureCors(corsConfig);

        std::vector<char> body;
        Request req(body);
        req.method_ = "GET";
        req.requestPath_ = "/api/users";
        req.headers_.push_back({"Origin", "https://anywhere.com"});

        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);
        HandlerResult handled = router.handle(req, rep);

        REQUIRE(handled == HandlerResult::Matched);
        REQUIRE(handlerCalled == true);
        REQUIRE(rep.getHeaderValue("Access-Control-Allow-Origin") == "*");
        REQUIRE(rep.getHeaderValue("Access-Control-Allow-Credentials").empty());
    }

    SECTION("should reject disallowed origin") {
        // Configure CORS with specific origins
        CorsConfig corsConfig;
        corsConfig.allowedOrigins.insert("https://allowed.com");
        router.configureCors(corsConfig);

        std::vector<char> body;
        Request req(body);
        req.method_ = "GET";
        req.requestPath_ = "/api/users";
        req.headers_.push_back({"Origin", "https://forbidden.com"});

        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);
        HandlerResult handled = router.handle(req, rep);

        REQUIRE(handled == HandlerResult::Matched);
        REQUIRE(handlerCalled == true);
        // No CORS headers should be added for disallowed origin
        REQUIRE(rep.getHeaderValue("Access-Control-Allow-Origin").empty());
    }

    SECTION("should handle same-origin requests (no Origin header)") {
        // Configure CORS
        CorsConfig corsConfig;
        corsConfig.allowedOrigins.insert("https://example.com");
        router.configureCors(corsConfig);

        std::vector<char> body;
        Request req(body);
        req.method_ = "GET";
        req.requestPath_ = "/api/users";
        // No Origin header = same-origin request

        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);
        HandlerResult handled = router.handle(req, rep);

        REQUIRE(handled == HandlerResult::Matched);
        REQUIRE(handlerCalled == true);
        // No CORS headers should be added for same-origin requests
        REQUIRE(rep.getHeaderValue("Access-Control-Allow-Origin").empty());
    }

    SECTION("should handle CORS preflight requests") {
        // Configure CORS
        CorsConfig corsConfig;
        corsConfig.allowedOrigins.insert("https://example.com");
        corsConfig.allowedHeaders.insert("Content-Type");
        corsConfig.allowedHeaders.insert("Authorization");
        corsConfig.allowCredentials = true;
        corsConfig.maxAge = 3600;
        router.configureCors(corsConfig);

        std::vector<char> body;
        Request req(body);
        req.method_ = "OPTIONS";
        req.requestPath_ = "/api/users";
        req.headers_.push_back({"Origin", "https://example.com"});
        req.headers_.push_back({"Access-Control-Request-Method", "POST"});
        req.headers_.push_back({"Access-Control-Request-Headers", "Content-Type, Authorization"});

        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);
        HandlerResult handled = router.handle(req, rep);

        REQUIRE(handled == HandlerResult::Matched);
        REQUIRE(handlerCalled == false);  // Handler should not be called for preflight
        // should not include safelisted headers like Content-Type
        REQUIRE(rep.getHeaderValue("Access-Control-Allow-Headers").find("Content-Type") ==
                std::string::npos);
        REQUIRE(rep.getHeaderValue("Access-Control-Allow-Origin") == "https://example.com");
        REQUIRE(rep.getHeaderValue("Access-Control-Allow-Methods").find("POST") !=
                std::string::npos);
        REQUIRE(rep.getHeaderValue("Access-Control-Allow-Methods").find("GET") !=
                std::string::npos);
        REQUIRE(rep.getHeaderValue("Access-Control-Allow-Methods").find("HEAD") !=
                std::string::npos);
        REQUIRE(rep.getHeaderValue("Access-Control-Allow-Headers").find("Authorization") !=
                std::string::npos);
        REQUIRE(rep.getHeaderValue("Access-Control-Max-Age") == "3600");
        REQUIRE(rep.getHeaderValue("Access-Control-Allow-Credentials") == "true");
        REQUIRE(rep.getStatus() == Reply::ok);
    }

    SECTION("should reject preflight for disallowed origin") {
        // Configure CORS with specific origins
        CorsConfig corsConfig;
        corsConfig.allowedOrigins.insert("https://allowed.com");
        router.configureCors(corsConfig);

        std::vector<char> body;
        Request req(body);
        req.method_ = "OPTIONS";
        req.requestPath_ = "/api/users";
        req.headers_.push_back({"Origin", "https://forbidden.com"});
        req.headers_.push_back({"Access-Control-Request-Method", "POST"});

        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);
        HandlerResult handled = router.handle(req, rep);

        REQUIRE(handled == HandlerResult::NoMatch);
        REQUIRE(handlerCalled == false);
    }

    SECTION("should reject preflight for unsupported method") {
        // Configure CORS
        CorsConfig corsConfig;
        corsConfig.allowedOrigins.insert("https://example.com");
        router.configureCors(corsConfig);

        std::vector<char> body;
        Request req(body);
        req.method_ = "OPTIONS";
        req.requestPath_ = "/api/users";
        req.headers_.push_back({"Origin", "https://example.com"});
        req.headers_.push_back({"Access-Control-Request-Method", "PATCH"});  // Not supported

        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);
        HandlerResult handled = router.handle(req, rep);

        REQUIRE(handled == HandlerResult::NoMatch);
        REQUIRE(handlerCalled == false);
    }

    SECTION("should handle preflight for non-existent path") {
        // Configure CORS
        CorsConfig corsConfig;
        corsConfig.allowedOrigins.insert("https://example.com");
        router.configureCors(corsConfig);

        std::vector<char> body;
        Request req(body);
        req.method_ = "OPTIONS";
        req.requestPath_ = "/api/nonexistent";
        req.headers_.push_back({"Origin", "https://example.com"});
        req.headers_.push_back({"Access-Control-Request-Method", "GET"});

        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);
        HandlerResult handled = router.handle(req, rep);

        REQUIRE(handled == HandlerResult::NoMatch);
        REQUIRE(handlerCalled == false);
    }

    SECTION("should handle preflight with wildcard origin") {
        // Configure CORS with wildcard
        CorsConfig corsConfig;
        corsConfig.allowedOrigins.insert("*");
        corsConfig.allowedHeaders.insert("Content-Type");
        corsConfig.allowCredentials = false;  // Cannot use credentials with wildcard
        router.configureCors(corsConfig);

        std::vector<char> body;
        Request req(body);
        req.method_ = "OPTIONS";
        req.requestPath_ = "/api/users";
        req.headers_.push_back({"Origin", "https://anywhere.com"});
        req.headers_.push_back({"Access-Control-Request-Method", "POST"});
        req.headers_.push_back({"Access-Control-Request-Headers", "Content-Type"});

        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);
        HandlerResult handled = router.handle(req, rep);

        REQUIRE(handled == HandlerResult::Matched);
        REQUIRE(handlerCalled == false);
        // should not include safelisted headers like Content-Type
        REQUIRE(rep.getHeaderValue("Access-Control-Allow-Headers").find("Content-Type") ==
                std::string::npos);
        REQUIRE(rep.getHeaderValue("Access-Control-Allow-Origin") == "*");
        REQUIRE(rep.getHeaderValue("Access-Control-Allow-Credentials").empty());
    }

    SECTION("should distinguish between CORS preflight and regular OPTIONS") {
        // Configure CORS
        CorsConfig corsConfig;
        corsConfig.allowedOrigins.insert("https://example.com");
        router.configureCors(corsConfig);

        // Regular OPTIONS request (no CORS headers)
        std::vector<char> body;
        Request req(body);
        req.method_ = "OPTIONS";
        req.requestPath_ = "/api/users";
        // No Origin or Access-Control-Request-Method headers

        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);
        HandlerResult handled = router.handle(req, rep);

        REQUIRE(handled == HandlerResult::Matched);
        REQUIRE(handlerCalled == false);
        // Should get regular Allow header, not CORS headers
        auto allowedMethods = rep.getHeaderValue("Allow");
        REQUIRE(split_methods(allowedMethods) == std::set<std::string>({"GET", "HEAD", "POST"}));
        REQUIRE(rep.getHeaderValue("Access-Control-Allow-Origin").empty());
        REQUIRE(rep.getHeaderValue("Access-Control-Allow-Methods").empty());
    }

    SECTION("should handle preflight with no requested headers") {
        // Configure CORS
        CorsConfig corsConfig;
        corsConfig.allowedOrigins.insert("https://example.com");
        router.configureCors(corsConfig);

        std::vector<char> body;
        Request req(body);
        req.method_ = "OPTIONS";
        req.requestPath_ = "/api/users";
        req.headers_.push_back({"Origin", "https://example.com"});
        req.headers_.push_back({"Access-Control-Request-Method", "GET"});
        // No Access-Control-Request-Headers

        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);
        HandlerResult handled = router.handle(req, rep);

        REQUIRE(handled == HandlerResult::Matched);
        REQUIRE(handlerCalled == false);
        REQUIRE(rep.getHeaderValue("Access-Control-Allow-Origin") == "https://example.com");
        REQUIRE(rep.getHeaderValue("Access-Control-Allow-Methods").find("GET") !=
                std::string::npos);
        // No Access-Control-Allow-Headers should be set when no headers were requested
    }

    SECTION("should only allow headers that are in the configuration") {
        // Configure CORS with specific allowed headers
        CorsConfig corsConfig;
        corsConfig.allowedOrigins.insert("https://example.com");
        corsConfig.allowedHeaders.insert("Content-Type");
        corsConfig.allowedHeaders.insert("Authorization");
        router.configureCors(corsConfig);

        std::vector<char> body;
        Request req(body);
        req.method_ = "OPTIONS";
        req.requestPath_ = "/api/users";
        req.headers_.push_back({"Origin", "https://example.com"});
        req.headers_.push_back({"Access-Control-Request-Method", "POST"});
        // Request both allowed and disallowed headers
        req.headers_.push_back({"Access-Control-Request-Headers",
                                "Content-Type, X-Custom-Header, Authorization, X-Evil-Header"});

        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);
        HandlerResult handled = router.handle(req, rep);

        REQUIRE(handled == HandlerResult::Matched);
        REQUIRE(handlerCalled == false);

        std::string allowedHeaders = rep.getHeaderValue("Access-Control-Allow-Headers");
        // Should only include the headers that are both requested AND in our allowed list
        // should not include safelisted headers like Content-Type
        REQUIRE(allowedHeaders.find("Content-Type") == std::string::npos);
        REQUIRE(allowedHeaders.find("Authorization") != std::string::npos);
        // Should NOT include the disallowed headers
        REQUIRE(allowedHeaders.find("X-Custom-Header") == std::string::npos);
        REQUIRE(allowedHeaders.find("X-Evil-Header") == std::string::npos);
    }

    SECTION("should handle preflight when no requested headers are allowed") {
        // Configure CORS with specific allowed headers
        CorsConfig corsConfig;
        corsConfig.allowedOrigins.insert("https://example.com");
        corsConfig.allowedHeaders.insert("Authorization");  // Only allow Authorization
        router.configureCors(corsConfig);

        std::vector<char> body;
        Request req(body);
        req.method_ = "OPTIONS";
        req.requestPath_ = "/api/users";
        req.headers_.push_back({"Origin", "https://example.com"});
        req.headers_.push_back({"Access-Control-Request-Method", "POST"});
        // Request only disallowed headers
        req.headers_.push_back(
            {"Access-Control-Request-Headers", "X-Custom-Header, X-Another-Header"});

        std::vector<char> sendBuffer(1024);
        Reply rep(sendBuffer);
        HandlerResult handled = router.handle(req, rep);

        REQUIRE(handled == HandlerResult::Matched);
        REQUIRE(handlerCalled == false);

        // Should not include Access-Control-Allow-Headers since none of the requested headers are
        // allowed
        REQUIRE(rep.getHeaderValue("Access-Control-Allow-Headers").empty());
    }
}
