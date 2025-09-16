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

        Reply rep(1024);

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

        Reply rep(1024);

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

        Reply rep(1024);

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

        Reply rep(1024);

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

        Reply rep(1024);

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

        Reply rep(1024);

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

        Reply rep(1024);

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

        Reply rep(1024);

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

        Reply rep(1024);

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

        Reply rep(1024);

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

        Reply rep(1024);

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

        Reply rep(1024);

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

        Reply rep(1024);

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
        Reply rep(1024);
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
        Reply rep(1024);
        HandlerResult handled = router.handle(req, rep);
        REQUIRE(handled == HandlerResult::Matched);
        REQUIRE(handlerCalled == false);  // Handler should not be called for OPTIONS
        auto allowedMethods = rep.getHeaderValue("Allow");
        REQUIRE(split_methods(allowedMethods) == std::set<std::string>({"GET", "HEAD", "POST"}));
        REQUIRE(rep.getStatus() == Reply::ok);
    }
}
