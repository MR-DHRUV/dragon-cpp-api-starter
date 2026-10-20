#pragma once

#include "OtelSpan.h"
#include "OtelMetrics.h"
#include "OtelLogger.h"
#include "ScopedSpan.h"

#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <memory>
#include <json/json.h>

namespace api::otel {

struct OtelConfig {
    bool enabled{true};
    std::string serviceName{"backend-server"};
    std::string serviceVersion{"1.0.0"};
    std::string collectorEndpoint{"http://127.0.0.1:4318"};
    int batchTimeoutMs{1000};
    size_t maxQueueSize{4096};
    bool exportTraces{true};
    bool exportMetrics{true};
    bool exportLogs{true};
};

class OtelManager {
public:
    static OtelManager& instance() {
        static OtelManager instance;
        return instance;
    }

    void initialize(const Json::Value& configRoot);
    void setupDrogonHooks();
    void shutdown();

    void enqueueSpan(OtelSpanPtr span);
    void enqueueLog(const OtelLogRecord& log);

    const OtelConfig& getConfig() const { return config_; }
    bool isEnabled() const { return config_.enabled; }

    // Child span helpers for Controller / Service / Database / sub-operations
    OtelSpanPtr startChildSpan(const std::string& name, SpanKind kind = SpanKind::Internal);
    OtelSpanPtr startChildSpan(const std::string& name, const OtelSpanPtr& parentSpan, SpanKind kind = SpanKind::Internal);

private:
    OtelManager() = default;
    ~OtelManager();

    void exporterWorkerLoop();
    void flushSpans(std::vector<OtelSpanPtr>& spansToExport);
    void flushLogs(std::vector<OtelLogRecord>& logsToExport);
    void flushMetrics();

    bool sendHttpJson(const std::string& urlPath, const std::string& jsonBody);

    OtelConfig config_;
    std::atomic<bool> running_{false};
    std::thread exporterThread_;

    std::mutex spanQueueMutex_;
    std::condition_variable spanCv_;
    std::deque<OtelSpanPtr> spanQueue_;

    std::mutex logQueueMutex_;
    std::deque<OtelLogRecord> logQueue_;

    std::chrono::steady_clock::time_point lastMetricsFlush_;
};

} // namespace api::otel
