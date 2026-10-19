#include "api/db/DatabaseManager.h"
#include <drogon/drogon.h>
#include <trantor/utils/Logger.h>

namespace api::db {

drogon::orm::DbClientPtr DatabaseManager::getClient() const {
    auto client = drogon::app().getDbClient("default");
    if (!client) {
        client = drogon::app().getFastDbClient("default");
    }
    return client;
}

void DatabaseManager::checkHealth(std::function<void(bool healthy, double latencyMs, const std::string& message)> callback) {
    auto client = getClient();
    if (!client) {
        callback(false, 0.0, "Database client 'default' is not configured or initialized");
        return;
    }

    auto startTime = std::chrono::steady_clock::now();

    *client << "SELECT 1;" 
        >> [startTime, callback = std::move(callback)](const drogon::orm::Result& r) {
            auto endTime = std::chrono::steady_clock::now();
            double latencyMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();
            if (!r.empty()) {
                callback(true, latencyMs, "PostgreSQL connection healthy");
            } else {
                callback(false, latencyMs, "Empty response from PostgreSQL");
            }
        }
        >> [callback = std::move(callback)](const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "PostgreSQL healthcheck failed: " << e.base().what();
            callback(false, 0.0, std::string("Database error: ") + e.base().what());
        };
}

} // namespace api::db
