#include <catch2/catch_test_macros.hpp>

#include "request.hpp"
#include "reply.hpp"
#include "http_result.hpp"

using namespace beauty;

TEST_CASE("HttpResult JSON response generation", "[http_result]") {
    std::vector<char> replyBuf;
    HttpResult result(replyBuf);

    SECTION("should create single key-value JSON response with string value") {
        result.singleJsonKeyValue("key1", "value1");
        std::string expected = "{\"key1\":\"value1\"}";
        REQUIRE(std::string(replyBuf.begin(), replyBuf.end()) == expected);
    }
    SECTION("should create single key-value JSON response with numeric value") {
        result.singleJsonKeyValue("key2", 42);
        std::string expected = "{\"key2\":42}";
        REQUIRE(std::string(replyBuf.begin(), replyBuf.end()) == expected);
    }
    SECTION("should create single key-value JSON response with boolean value") {
        result.singleJsonKeyValue("key3", true);
        std::string expected = "{\"key3\":true}";
        REQUIRE(std::string(replyBuf.begin(), replyBuf.end()) == expected);
    }
    SECTION("should build JSON response using a builder function") {
        result.buildJsonResponse([]() -> cJSON* {
            cJSON* root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "key4", "value4");
            cJSON_AddNumberToObject(root, "key5", 123);
            return root;
        });
        std::string expected = "{\"key4\":\"value4\",\"key5\":123}";
        REQUIRE(std::string(replyBuf.begin(), replyBuf.end()) == expected);
    }
    SECTION("should set pre-built cJSON object as response") {
        cJSON* root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "key6", "value6");
        result.setJsonResponse(root);
        std::string expected = "{\"key6\":\"value6\"}";
        REQUIRE(std::string(replyBuf.begin(), replyBuf.end()) == expected);
    }
    SECTION("should handle empty response") {
        result.setJsonResponse(nullptr);
        REQUIRE(std::string(replyBuf.begin(), replyBuf.end()).empty());
    }
}

TEST_CASE("HttpResult parse JSON request", "[http_result]") {
    std::vector<char> replyBuf;
    HttpResult result(replyBuf);

    std::vector<char> requestBuf;
    const std::string jsonStr = "{\"key1\":\"value1\",\"key2\":42,\"key3\":true}";
    requestBuf.insert(requestBuf.end(), jsonStr.begin(), jsonStr.end());

    SECTION("should parse JSON request correctly") {
        REQUIRE(result.parseJsonRequest(requestBuf));
        REQUIRE(result.containsKey("key1"));

        REQUIRE(result.getString("key1") == "value1");
        REQUIRE(result.requestBody_.as<std::string>("key1") == "value1");

        REQUIRE(result.getString("key1") == "value1");
        const char* val = result.requestBody_.as<const char*>("key1");
        REQUIRE(std::string(val) == "value1");

        REQUIRE(result.containsKey("key2"));
        REQUIRE(result.getInt("key2") == 42);
        REQUIRE(result.requestBody_.as<int>("key2") == 42);

        REQUIRE(result.containsKey("key3"));
        REQUIRE(result.getBool("key3") == true);
        REQUIRE(result.requestBody_.as<bool>("key3") == true);
    }
    SECTION("should return default value for missing keys") {
        REQUIRE(result.getString("missingKey", "default") == "default");
        REQUIRE(result.getInt("missingKey", 100) == 100);
        REQUIRE(result.getBool("missingKey", false) == false);
    }
    SECTION("should return false for invalid JSON") {
        requestBuf = {'{', 'i', 'n', 'v', 'a', 'l', 'i', 'd', '}'};
        REQUIRE_FALSE(result.parseJsonRequest(requestBuf));
        REQUIRE(result.requestBody_.isEmpty());
    }
    SECTION("should handle invalid empty body") {
        requestBuf.clear();
        REQUIRE_FALSE(result.parseJsonRequest(requestBuf));
        REQUIRE(result.isRequestEmpty());
    }
    SECTION("should handle valid empty JSON body") {
        requestBuf = {'{', ' ', '}'};
        REQUIRE(result.parseJsonRequest(requestBuf));
        REQUIRE(result.isRequestEmpty());
    }
}

TEST_CASE("HttpResult streaming operator", "[http_result]") {
    std::vector<char> replyBuf;
    HttpResult result(replyBuf);

    result << "Hello, ";
    result << "World!";
    std::string expected = "Hello, World!";
    REQUIRE(std::string(replyBuf.begin(), replyBuf.end()) == expected);
}

TEST_CASE("HttpResult error handling", "[http_result]") {
    std::vector<char> replyBuf;
    HttpResult result(replyBuf);

    SECTION("should set error response") {
        result.jsonError(404, "Not Found");
        std::string expected = "{\"error\":\"Not Found\"}";
        REQUIRE(std::string(replyBuf.begin(), replyBuf.end()) == expected);
        REQUIRE(result.statusCode_ == beauty::Reply::status_type::not_found);
    }
    SECTION("should handle empty error message") {
        result.jsonError(500, "");
        std::string expected = "{\"error\":\"\"}";
        REQUIRE(std::string(replyBuf.begin(), replyBuf.end()) == expected);
        REQUIRE(result.statusCode_ == beauty::Reply::status_type::internal_server_error);
    }
}
