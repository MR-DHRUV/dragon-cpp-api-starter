#include "api/controllers/HealthController.h"
#include "api/db/DatabaseManager.h"
#include "api/common/ApiResponse.h"
#include "api/common/RapidJsonHelper.h"

#include <drogon/drogon.h>
#include <chrono>
#include <ctime>

namespace api::controllers {

static const auto g_startTime = std::chrono::steady_clock::now();

void HealthController::healthCheck(
    const drogon::HttpRequestPtr& /*req*/,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) 
{
    db::DatabaseManager::instance().checkHealth(
        [callback = std::move(callback)](bool healthy, double latencyMs, const std::string& message) {
            auto now = std::chrono::steady_clock::now();
            auto uptimeSec = std::chrono::duration_cast<std::chrono::seconds>(now - g_startTime).count();

            auto resp = common::ApiResponse::successWithPayload(
                [healthy, latencyMs, &message, uptimeSec](rapidjson::Writer<rapidjson::StringBuffer>& writer) {
                    writer.StartObject();
                    
                    writer.Key("status");
                    writer.String(healthy ? "UP" : "DEGRADED");

                    writer.Key("uptimeSeconds");
                    writer.Int64(uptimeSec);

                    writer.Key("threads");
                    writer.Uint64(static_cast<uint64_t>(drogon::app().getThreadNum()));

                    writer.Key("database");
                    writer.StartObject();
                    writer.Key("connected");
                    writer.Bool(healthy);
                    writer.Key("latencyMs");
                    writer.Double(latencyMs);
                    writer.Key("detail");
                    writer.String(message.c_str(), static_cast<rapidjson::SizeType>(message.length()));
                    writer.EndObject();

                    writer.EndObject();
                },
                "System health report",
                healthy ? drogon::k200OK : drogon::k503ServiceUnavailable
            );

            callback(resp);
        });
}

void HealthController::ping(
    const drogon::HttpRequestPtr& /*req*/,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) 
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

    auto now = std::chrono::system_clock::now();
    auto epochMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    writer.StartObject();
    writer.Key("pong");
    writer.Bool(true);
    writer.Key("timestamp");
    writer.Int64(epochMs);
    writer.EndObject();

    callback(common::RapidJsonHelper::toHttpResponse(buffer, drogon::k200OK));
}

} // namespace api::controllers
