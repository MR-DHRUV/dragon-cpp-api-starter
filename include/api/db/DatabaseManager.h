#pragma once

#include <drogon/orm/DbClient.h>
#include <functional>
#include <string>
#include <chrono>

namespace api::db {

class DatabaseManager {
public:
    static DatabaseManager& instance() {
        static DatabaseManager instance;
        return instance;
    }

    /**
     * @brief Gets the configured Drogon DbClient for PostgreSQL.
     */
    drogon::orm::DbClientPtr getClient() const;

    /**
     * @brief Asynchronously tests the database connection and returns latency in microseconds.
     */
    void checkHealth(std::function<void(bool healthy, double latencyMs, const std::string& message)> callback);

private:
    DatabaseManager() = default;
    ~DatabaseManager() = default;
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
};

} // namespace api::db
