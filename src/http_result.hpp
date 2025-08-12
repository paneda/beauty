#pragma once

#include <functional>
#include <istream>
#include <string>
#include <sstream>
#include <vector>

#include "cJSON.h"
#include "beauty_common.hpp"
#include "reply.hpp"

namespace beauty {

// HttpResult makes it convenient to build the reply back to client.
// Constructor takes a reference to the reply::content_ buffer, then use either
// streaming operator '<<' to add body data directly into it or use the
// cJSON-based methods to build JSON responses.
// In addition, parse JSON requests and access request data in a type-safe
// manner.
class HttpResult {
   public:
    class JsonDocument {
       public:
        // Default constructor creates an empty document
        JsonDocument() : jsonRoot_(nullptr) {}

        // Constructor with root node
        JsonDocument(cJSON* root) : jsonRoot_(root) {}

        bool containsKey(const std::string& key) const {
            if (!jsonRoot_)
                return false;
            return cJSON_GetObjectItem(jsonRoot_, key.c_str()) != nullptr;
        }

        bool isEmpty() const {
            if (!jsonRoot_)
                return true;

            // Check if the object has any properties
            if (cJSON_IsObject(jsonRoot_) && !cJSON_GetArraySize(jsonRoot_)) {
                return true;
            }

            // Check if array has elements
            if (cJSON_IsArray(jsonRoot_) && !cJSON_GetArraySize(jsonRoot_)) {
                return true;
            }

            return false;
        }

        // Template for type-based value access
        template <typename T>
        T as(const std::string& key) const;

        // Update the root pointer
        void setRoot(cJSON* root) {
            jsonRoot_ = root;
        }

       private:
        cJSON* jsonRoot_;
    };

    HttpResult(std::vector<char>& replyBuf)
        : replyBuf_(replyBuf), requestBody_(), requestRoot_(nullptr), responseRoot_(nullptr) {}

    ~HttpResult() {
        if (requestRoot_) {
            cJSON_Delete(requestRoot_);
        }
        if (responseRoot_) {
            cJSON_Delete(responseRoot_);
        }
    }

    // Streaming operator for various types
    HttpResult& operator<<(const std::string& val) {
        replyBuf_.insert(replyBuf_.end(), val.begin(), val.end());
        return *this;
    }

    //===== cJSON-based request parsing methods =====//

    bool parseJsonRequest(const std::vector<char>& requestBuf) {
        // Clean up previous JSON object if it exists
        if (requestRoot_) {
            cJSON_Delete(requestRoot_);
            requestRoot_ = nullptr;
        }

        // Ensure buffer is null-terminated for cJSON
        std::string jsonStr(requestBuf.begin(), requestBuf.end());

        // Parse JSON
        requestRoot_ = cJSON_Parse(jsonStr.c_str());
        if (requestRoot_ == nullptr) {
            const char* errorPtr = cJSON_GetErrorPtr();

            std::ostringstream oss;
            oss << "JSON parsing failed";

            if (errorPtr != nullptr) {
                ptrdiff_t pos = errorPtr - jsonStr.c_str() - 1;
                oss << " at position " << (pos >= 0 ? static_cast<size_t>(pos) : 0);
            }

            jsonError(400, oss.str());
            return false;
        }

        // Update the requestBody_ to point to our parsed JSON
        requestBody_.setRoot(requestRoot_);

        return true;
    }

    // Check if a key exists in the parsed JSON
    bool containsKey(const std::string& key) const {
        if (!requestRoot_)
            return false;
        return cJSON_GetObjectItem(requestRoot_, key.c_str()) != nullptr;
    }

    bool isRequestEmpty() const {
        return requestBody_.isEmpty();
    }

    // Get value as string
    std::string getString(const std::string& key, const std::string& defaultVal = "") const {
        if (!requestRoot_)
            return defaultVal;

        cJSON* item = cJSON_GetObjectItem(requestRoot_, key.c_str());
        if (cJSON_IsString(item)) {
            return std::string(item->valuestring);
        }
        return defaultVal;
    }

    // Get value as integer
    int getInt(const std::string& key, int defaultVal = 0) const {
        if (!requestRoot_)
            return defaultVal;

        cJSON* item = cJSON_GetObjectItem(requestRoot_, key.c_str());
        if (cJSON_IsNumber(item)) {
            return item->valueint;
        }
        return defaultVal;
    }

    // Get value as boolean
    bool getBool(const std::string& key, bool defaultVal = false) const {
        if (!requestRoot_)
            return defaultVal;

        cJSON* item = cJSON_GetObjectItem(requestRoot_, key.c_str());
        if (cJSON_IsBool(item)) {
            return cJSON_IsTrue(item);
        }
        return defaultVal;
    }

    // Get value as double
    double getDouble(const std::string& key, double defaultVal = 0.0) const {
        if (!requestRoot_)
            return defaultVal;

        cJSON* item = cJSON_GetObjectItem(requestRoot_, key.c_str());
        if (cJSON_IsNumber(item)) {
            return item->valuedouble;
        }
        return defaultVal;
    }

    //===== cJSON-based response generation methods =====//

    // Create a single key-value JSON response with string value
    void singleJsonKeyValue(const std::string& key, const std::string& value) {
        cJSON* root = startJson();
        cJSON_AddStringToObject(root, key.c_str(), value.c_str());
        serializeResponse();
    }

    void singleJsonKeyValue(const std::string& key, const char* value) {
        cJSON* root = startJson();
        cJSON_AddStringToObject(root, key.c_str(), value);
        serializeResponse();
    }

    // Create a single key-value JSON response with numeric value
    template <typename T>
    void singleJsonKeyValue(const std::string& key, T value) {
        cJSON* root = startJson();
        cJSON_AddNumberToObject(root, key.c_str(), static_cast<double>(value));
        serializeResponse();
    }

    // Create a single key-value JSON response with boolean value
    void singleJsonKeyValue(const std::string& key, bool value) {
        cJSON* root = startJson();
        cJSON_AddBoolToObject(root, key.c_str(), value);
        serializeResponse();
    }

    // Create an error response
    void jsonError(int statusCode, const std::string& message) {
        statusCode_ = static_cast<beauty::Reply::status_type>(statusCode);
        singleJsonKeyValue("error", message);
    }

    // Generate a JSON response using a builder function
    void buildJsonResponse(std::function<cJSON*()> builder) {
        clearResponse();
        // Let the builder create and return the root object
        responseRoot_ = builder();
        serializeResponse();
    }

    // Directly set a pre-built cJSON object as the response
    void setJsonResponse(cJSON* root) {
        clearResponse();
        // Take ownership of the provided cJSON object
        responseRoot_ = root;
        serializeResponse();
    }

    std::vector<char>& replyBuf_;
    JsonDocument requestBody_;
    beauty::Reply::status_type statusCode_ = beauty::Reply::status_type::ok;

   private:
    cJSON* requestRoot_;   // For parsing requests
    cJSON* responseRoot_;  // For generating responses

    // Clear any existing response
    void clearResponse() {
        replyBuf_.clear();
        if (responseRoot_) {
            cJSON_Delete(responseRoot_);
            responseRoot_ = nullptr;
        }
    }

    // Start a new JSON response object
    cJSON* startJson() {
        clearResponse();
        responseRoot_ = cJSON_CreateObject();
        return responseRoot_;
    }

    // Convert current response object to JSON string and place in replyBuf_
    void serializeResponse() {
        if (responseRoot_) {
            char* json_str = cJSON_PrintUnformatted(responseRoot_);
            if (json_str) {
                replyBuf_.clear();
                replyBuf_.insert(replyBuf_.end(), json_str, json_str + strlen(json_str));
                free(json_str);
            }
        }
    }
};

// Template specializations for JsonDocument::as
template <>
inline std::string HttpResult::JsonDocument::as<std::string>(const std::string& key) const {
    if (!jsonRoot_)
        return "";

    cJSON* item = cJSON_GetObjectItem(jsonRoot_, key.c_str());
    if (cJSON_IsString(item)) {
        return std::string(item->valuestring);
    }
    return "";
}

template <>
inline int HttpResult::JsonDocument::as<int>(const std::string& key) const {
    if (!jsonRoot_)
        return 0;

    cJSON* item = cJSON_GetObjectItem(jsonRoot_, key.c_str());
    if (cJSON_IsNumber(item)) {
        return item->valueint;
    }
    return 0;
}

template <>
inline bool HttpResult::JsonDocument::as<bool>(const std::string& key) const {
    if (!jsonRoot_)
        return false;

    cJSON* item = cJSON_GetObjectItem(jsonRoot_, key.c_str());
    if (cJSON_IsBool(item)) {
        return cJSON_IsTrue(item);
    }
    return false;
}

template <>
inline double HttpResult::JsonDocument::as<double>(const std::string& key) const {
    if (!jsonRoot_)
        return 0.0;

    cJSON* item = cJSON_GetObjectItem(jsonRoot_, key.c_str());
    if (cJSON_IsNumber(item)) {
        return item->valuedouble;
    }
    return 0.0;
}

template <>
inline const char* HttpResult::JsonDocument::as<const char*>(const std::string& key) const {
    if (!jsonRoot_)
        return "";

    cJSON* item = cJSON_GetObjectItem(jsonRoot_, key.c_str());
    if (cJSON_IsString(item)) {
        return item->valuestring;
    }
    return "";
}

}  // namespace beauty
