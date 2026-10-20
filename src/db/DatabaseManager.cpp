#include "api/db/DatabaseManager.h"
#include <drogon/drogon.h>
#include <trantor/utils/Logger.h>
#include <chrono>
#include <memory>

namespace api::db {

drogon::orm::DbClientPtr DatabaseManager::getClient() const {
    auto client = drogon::app().getDbClient("default");
    if (!client) {
        client = drogon::app().getFastDbClient("default");
    }
    return client;
}

void DatabaseManager::checkHealth(
    std::function<void(bool healthy, double latencyMs, const std::string& message)> callback,
    const otel::SpanPtr& parentSpan) 
{
    auto client = getClient();
    if (!client) {
        callback(false, 0.0, "Database client 'default' is not configured or initialized");
        return;
    }

    auto cbPtr = std::make_shared<std::function<void(bool, double, const std::string&)>>(std::move(callback));
    auto startTime = std::chrono::steady_clock::now();
    tracedExec(
        client,
        parentSpan,
        "SELECT 1",
        "SELECT 1;",
        [startTime, cbPtr](const drogon::orm::Result& r) {
            auto endTime = std::chrono::steady_clock::now();
            double latencyMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

            if (cbPtr && *cbPtr) {
                if (!r.empty()) {
                    (*cbPtr)(true, latencyMs, "PostgreSQL connection healthy");
                } else {
                    (*cbPtr)(false, latencyMs, "Empty response from PostgreSQL");
                }
            }
        },
        [cbPtr](const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "PostgreSQL healthcheck failed: " << e.base().what();
            if (cbPtr && *cbPtr) {
                (*cbPtr)(false, 0.0, std::string("Database error: ") + e.base().what());
            }
        }
    );
}

} // namespace api::db
