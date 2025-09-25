#include <catch2/catch_test_macros.hpp>

#include "beauty/request.hpp"
#include "beauty/reply.hpp"
#include "beauty/http_result.hpp"

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
        REQUIRE(result.statusCode_ == beauty::Reply::status_type::bad_request);
        REQUIRE(std::string(replyBuf.begin(), replyBuf.end()) ==
                "{\"status\":400,\"message\":\"JSON parsing failed at position 1\"}");
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

TEST_CASE("HttpResult JsonDocument getRoot for complex JSON operations", "[http_result]") {
    std::vector<char> replyBuf;
    HttpResult result(replyBuf);

    // Create a complex nested JSON structure
    const std::string jsonStr = R"({
        "user": {
            "name": "John Doe",
            "age": 30,
            "address": {
                "street": "123 Main St",
                "city": "Anytown"
            }
        },
        "items": [
            {"id": 1, "name": "item1"},
            {"id": 2, "name": "item2"}
        ]
    })";

    std::vector<char> requestBuf(jsonStr.begin(), jsonStr.end());

    SECTION("should provide direct access to cJSON root for complex operations") {
        REQUIRE(result.parseJsonRequest(requestBuf));

        // Get the root cJSON object
        cJSON* root = result.requestBody_.getRoot();
        REQUIRE(root != nullptr);

        // Access nested object using cJSON functions
        cJSON* user = cJSON_GetObjectItem(root, "user");
        REQUIRE(user != nullptr);
        REQUIRE(cJSON_IsObject(user));

        cJSON* address = cJSON_GetObjectItem(user, "address");
        REQUIRE(address != nullptr);
        REQUIRE(cJSON_IsObject(address));

        cJSON* street = cJSON_GetObjectItem(address, "street");
        REQUIRE(street != nullptr);
        REQUIRE(cJSON_IsString(street));
        REQUIRE(std::string(street->valuestring) == "123 Main St");

        // Access array using cJSON functions
        cJSON* items = cJSON_GetObjectItem(root, "items");
        REQUIRE(items != nullptr);
        REQUIRE(cJSON_IsArray(items));
        REQUIRE(cJSON_GetArraySize(items) == 2);

        cJSON* firstItem = cJSON_GetArrayItem(items, 0);
        REQUIRE(firstItem != nullptr);
        REQUIRE(cJSON_IsObject(firstItem));

        cJSON* itemId = cJSON_GetObjectItem(firstItem, "id");
        REQUIRE(itemId != nullptr);
        REQUIRE(cJSON_IsNumber(itemId));
        REQUIRE(itemId->valueint == 1);
    }

    SECTION("should return nullptr when no JSON has been parsed") {
        // Don't parse any JSON
        cJSON* root = result.requestBody_.getRoot();
        REQUIRE(root == nullptr);
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
        std::string expected = "{\"status\":404,\"message\":\"Not Found\"}";
        REQUIRE(std::string(replyBuf.begin(), replyBuf.end()) == expected);
        REQUIRE(result.statusCode_ == beauty::Reply::status_type::not_found);
    }
    SECTION("should handle empty error message") {
        result.jsonError(500, "");
        std::string expected = "{\"status\":500,\"message\":\"\"}";
        REQUIRE(std::string(replyBuf.begin(), replyBuf.end()) == expected);
        REQUIRE(result.statusCode_ == beauty::Reply::status_type::internal_server_error);
    }
}
