#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <chrono>

namespace api::otel {

struct RouteMetrics {
    std::string method;
    std::string route;
    std::unordered_map<int, uint64_t> statusCounts; // statusCode -> count
    uint64_t totalRequests{0};
    double totalDurationMs{0.0};
    std::vector<uint64_t> bucketCounts; // Count in each latency bucket
};

struct DbMetrics {
    std::unordered_map<std::string, uint64_t> operationCounts; // operation -> count
    std::atomic<uint64_t> totalSuccess{0};
    std::atomic<uint64_t> totalFailure{0};
    double totalDurationMs{0.0};
};

class OtelMetrics {
public:
    static OtelMetrics& instance() {
        static OtelMetrics instance;
        return instance;
    }

    void recordHttpRequest(const std::string& method, const std::string& route, int statusCode, double durationMs);
    void recordDbQuery(const std::string& operation, double durationMs, bool success);

    std::string toPrometheusText() const;
    std::string toOtlpJson(const std::string& serviceName, const std::string& serviceVersion) const;

    const std::vector<double>& getLatencyBounds() const { return latencyBounds_; }

private:
    OtelMetrics();
    ~OtelMetrics() = default;

    std::vector<double> latencyBounds_{1.0, 5.0, 10.0, 25.0, 50.0, 100.0, 250.0, 500.0, 1000.0, 2500.0, 5000.0};
    mutable std::mutex metricsMutex_;
    std::unordered_map<std::string, RouteMetrics> httpMetricsMap_; // "METHOD ROUTE" -> RouteMetrics
    DbMetrics dbMetrics_;
    std::chrono::system_clock::time_point startTime_;
};

} // namespace api::otel
