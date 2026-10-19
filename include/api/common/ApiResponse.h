#pragma once

#include "RapidJsonHelper.h"
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <drogon/HttpResponse.h>

#include <string>
#include <functional>

namespace api::common {

class ApiResponse {
public:
    /**
     * @brief Generates an ultra-fast standardized error response.
     */
    static drogon::HttpResponsePtr error(
        drogon::HttpStatusCode statusCode,
        const std::string& message) 
    {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

        writer.StartObject();
        writer.Key("success");
        writer.Bool(false);
        writer.Key("status");
        writer.Int(static_cast<int>(statusCode));
        writer.Key("error");
        writer.String(message.c_str(), static_cast<rapidjson::SizeType>(message.length()));
        writer.EndObject();

        return RapidJsonHelper::toHttpResponse(buffer, statusCode);
    }

    /**
     * @brief Generates a success response with a custom payload writer callback.
     * Keeps memory allocations near zero by streaming directly to the buffer.
     */
    static drogon::HttpResponsePtr successWithPayload(
        const std::function<void(rapidjson::Writer<rapidjson::StringBuffer>&)>& payloadWriter,
        const std::string& message = "OK",
        drogon::HttpStatusCode statusCode = drogon::k200OK) 
    {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

        writer.StartObject();
        writer.Key("success");
        writer.Bool(true);
        writer.Key("status");
        writer.Int(static_cast<int>(statusCode));
        writer.Key("message");
        writer.String(message.c_str(), static_cast<rapidjson::SizeType>(message.length()));
        
        writer.Key("data");
        payloadWriter(writer);

        writer.EndObject();

        return RapidJsonHelper::toHttpResponse(buffer, statusCode);
    }

    /**
     * @brief Simple success response without extra data payload.
     */
    static drogon::HttpResponsePtr success(
        const std::string& message = "Operation completed successfully",
        drogon::HttpStatusCode statusCode = drogon::k200OK) 
    {
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

        writer.StartObject();
        writer.Key("success");
        writer.Bool(true);
        writer.Key("status");
        writer.Int(static_cast<int>(statusCode));
        writer.Key("message");
        writer.String(message.c_str(), static_cast<rapidjson::SizeType>(message.length()));
        writer.EndObject();

        return RapidJsonHelper::toHttpResponse(buffer, statusCode);
    }
};

} // namespace api::common
