#pragma once

#include "api/otel/ScopedSpan.h"
#include "api/otel/OtelMetrics.h"
#include <drogon/orm/DbClient.h>
#include <functional>
#include <string>
#include <chrono>
#include <memory>
#include <utility>

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
     * @brief Asynchronously tests the database connection and returns latency in milliseconds.
     */
    void checkHealth(
        std::function<void(bool healthy, double latencyMs, const std::string& message)> callback,
        const otel::SpanPtr& parentSpan = nullptr);

private:
    DatabaseManager() = default;
    ~DatabaseManager() = default;
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
};

/**
 * @brief Wraps Drogon DbClient::execSqlAsync with OpenTelemetry Client span,
 *        timing, rows_affected attribute, and metrics recording.
 */
template <typename OnOk, typename OnErr, typename... Args>
void tracedExec(
    const drogon::orm::DbClientPtr& db,
    const otel::SpanPtr& parentSpan,
    const std::string& op,
    const std::string& sql,
    OnOk&& onOk,
    OnErr&& onErr,
    Args&&... args)
{
    auto span = std::make_shared<otel::ScopedSpan>("db.query " + op, parentSpan, otel::SpanKind::Client);
    span->attr("db.system", "postgresql");
    span->attr("db.statement", sql);

    if (!db) {
        span->fail("Database client is not available");
        span->end();
        otel::OtelMetrics::instance().recordDbQuery(op, 0.0, false);
        return;
    }

    auto startTime = std::chrono::steady_clock::now();

    db->execSqlAsync(
        sql,
        [span, startTime, op, onOk = std::forward<OnOk>(onOk)](const drogon::orm::Result& result) {
            auto endTime = std::chrono::steady_clock::now();
            double latencyMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

            int64_t rows = (result.affectedRows() > 0 && result.size() == 0)
                ? static_cast<int64_t>(result.affectedRows())
                : static_cast<int64_t>(result.size());
            span->attr("db.rows_affected", rows);
            span->end();

            otel::OtelMetrics::instance().recordDbQuery(op, latencyMs, true);

            onOk(result);
        },
        [span, op, onErr = std::forward<OnErr>(onErr)](const drogon::orm::DrogonDbException& e) {
            span->fail(e.base().what());
            span->end();

            otel::OtelMetrics::instance().recordDbQuery(op, 0.0, false);

            onErr(e);
        },
        std::forward<Args>(args)...
    );
}

} // namespace api::db
