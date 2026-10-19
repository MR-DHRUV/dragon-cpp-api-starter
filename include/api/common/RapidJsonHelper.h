#pragma once

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <rapidjson/error/en.h>
#include <drogon/HttpResponse.h>
#include <drogon/HttpTypes.h>

#include <string>
#include <string_view>
#include <optional>
#include <memory>

namespace api::common {

/**
 * @brief Helper utilities for RapidJSON and Drogon HTTP responses.
 * Designed for zero/minimal allocations, SIMD-friendly parsing, and clean error handling.
 */
class RapidJsonHelper {
public:
    /**
     * @brief Creates a Drogon HTTP response directly from a RapidJSON StringBuffer.
     * Fast-paths the JSON header and content body.
     */
    static drogon::HttpResponsePtr toHttpResponse(
        const rapidjson::StringBuffer& buffer,
        drogon::HttpStatusCode statusCode = drogon::k200OK) 
    {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(statusCode);
        resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        resp->setBody(std::string(buffer.GetString(), buffer.GetLength()));
        return resp;
    }

    /**
     * @brief Parses raw JSON string into a RapidJSON Document.
     * Returns std::nullopt and populates errorMessage on parse failure.
     */
    static std::optional<rapidjson::Document> parse(
        std::string_view jsonStr, 
        std::string* errorMessage = nullptr) 
    {
        rapidjson::Document doc;
        doc.Parse(jsonStr.data(), jsonStr.size());

        if (doc.HasParseError()) {
            if (errorMessage) {
                *errorMessage = rapidjson::GetParseError_En(doc.GetParseError());
            }
            return std::nullopt;
        }

        return doc;
    }

    /**
     * @brief Safely extracts a string field if it exists.
     */
    static std::optional<std::string> getString(const rapidjson::Value& value, const char* memberName) {
        if (value.HasMember(memberName) && value[memberName].IsString()) {
            return std::string(value[memberName].GetString(), value[memberName].GetStringLength());
        }
        return std::nullopt;
    }

    /**
     * @brief Safely extracts an int64 field if it exists.
     */
    static std::optional<int64_t> getInt64(const rapidjson::Value& value, const char* memberName) {
        if (value.HasMember(memberName) && value[memberName].IsInt64()) {
            return value[memberName].GetInt64();
        }
        return std::nullopt;
    }

    /**
     * @brief Safely extracts an int32 field if it exists.
     */
    static std::optional<int32_t> getInt(const rapidjson::Value& value, const char* memberName) {
        if (value.HasMember(memberName) && value[memberName].IsInt()) {
            return value[memberName].GetInt();
        }
        return std::nullopt;
    }

    /**
     * @brief Safely extracts a double field if it exists.
     */
    static std::optional<double> getDouble(const rapidjson::Value& value, const char* memberName) {
        if (value.HasMember(memberName) && value[memberName].IsNumber()) {
            return value[memberName].GetDouble();
        }
        return std::nullopt;
    }

    /**
     * @brief Safely extracts a bool field if it exists.
     */
    static std::optional<bool> getBool(const rapidjson::Value& value, const char* memberName) {
        if (value.HasMember(memberName) && value[memberName].IsBool()) {
            return value[memberName].GetBool();
        }
        return std::nullopt;
    }
};

} // namespace api::common
