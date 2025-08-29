#include <iostream>
#include <cassert>
#include <catch2/catch_test_macros.hpp>

#include "beauty/router.hpp"

using namespace beauty;

TEST_CASE("router functionality", "[lightweight_router]") {
	Router router;
	bool handlerCalled = false;
	std::map<std::string, std::string> capturedParams;

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

	// Create a mock reply
	Reply rep(1024);

	// Test the router
	HandlerResult handled = router.handle(req, rep);

	// Verify results
	REQUIRE(handled == HandlerResult::Matched);
	REQUIRE(handlerCalled == true);
	REQUIRE(capturedParams.size() == 1);
	REQUIRE(capturedParams["userId"] == "123");
}
