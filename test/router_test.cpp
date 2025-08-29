#include <iostream>
#include <cassert>
#include <catch2/catch_test_macros.hpp>

#include "beauty/router.hpp"

using namespace beauty;

TEST_CASE("router functionality", "[router]") {
	Router router;
	bool handlerCalled = false;
	std::map<std::string, std::string> capturedParams;

	SECTION("should match route with parameter and extract param") {
		// Add a test route
		router.addRoute("GET", "/users/{userId}", 
			[&](const Request&, Reply&, const std::map<std::string, std::string>& params) {
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
	SECTION("should return MethodNotSupported for existing path with different method") {
		// Add a test route
		router.addRoute("GET", "/users/{userId}", 
			[&](const Request&, Reply&, const std::map<std::string, std::string>&) {});

		// Create a mock request with different method
		std::vector<char> body;
		Request req(body);
		req.method_ = "DELETE";
		req.requestPath_ = "/users/123";

		Reply rep(1024);

		HandlerResult handled = router.handle(req, rep);

		REQUIRE(handled == HandlerResult::MethodNotSupported);
		REQUIRE(handlerCalled == false);
	}
	SECTION("should return NoMatch for non-existing path") {
		// Add a test route
		router.addRoute("GET", "/users/{userId}", 
			[&](const Request&, Reply&, const std::map<std::string, std::string>&) {});
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
		router.addRoute("GET", "/status", 
			[&](const Request&, Reply&, const std::map<std::string, std::string>&) {
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
		router.addRoute("GET", "/users/{userId}/posts/{postId}", 
			[&](const Request&, Reply&, const std::map<std::string, std::string>&) {
				handlerCalled = true;
			});

		// Create a mock request with partial path
		std::vector<char> body;
		Request req(body);
		req.method_ = "GET";
		req.requestPath_ = "/users/123/posts"; // Missing postId

		Reply rep(1024);

		HandlerResult handled = router.handle(req, rep);

		REQUIRE(handled == HandlerResult::NoMatch);
		REQUIRE(handlerCalled == false);
	}
	SECTION("should match complex paths with multiple parameters") {
		// Add a test route
		router.addRoute("GET", "/users/{userId}/posts/{postId}", 
			[&](const Request&, Reply&, const std::map<std::string, std::string>& params) {
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
	SECTION("should handle root path") {
		// Add a test route
		router.addRoute("GET", "/", 
			[&](const Request&, Reply&, const std::map<std::string, std::string>&) {
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
	SECTION("should handle trailing slashes consistently") {
		// Add a test route without trailing slash
		router.addRoute("GET", "/api/resource", 
			[&](const Request&, Reply&, const std::map<std::string, std::string>&) {
				handlerCalled = true;
			});

		// Create a mock request with trailing slash
		std::vector<char> body;
		Request req(body);
		req.method_ = "GET";
		req.requestPath_ = "/api/resource/";

		Reply rep(1024);

		HandlerResult handled = router.handle(req, rep);

		REQUIRE(handled == HandlerResult::NoMatch);
		REQUIRE(handlerCalled == false);

		// Reset and test without trailing slash
		handlerCalled = false;
		req.requestPath_ = "/api/resource";

		handled = router.handle(req, rep);

		REQUIRE(handled == HandlerResult::Matched);
		REQUIRE(handlerCalled == true);
	}
}
