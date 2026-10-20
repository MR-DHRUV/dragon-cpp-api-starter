#include "api/otel/OtelMetrics.h"
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <sstream>
#include <iomanip>

namespace api::otel {

OtelMetrics::OtelMetrics()
    : startTime_(std::chrono::system_clock::now())
{
}

void OtelMetrics::recordHttpRequest(
    const std::string& method,
    const std::string& route,
    int statusCode,
    double durationMs)
{
    std::lock_guard<std::mutex> lock(metricsMutex_);

    std::string key = method + " " + route;
    auto& rm = httpMetricsMap_[key];
    if (rm.bucketCounts.empty()) {
        rm.method = method;
        rm.route = route;
        rm.bucketCounts.resize(latencyBounds_.size() + 1, 0); // +1 for +Inf
    }

    rm.statusCounts[statusCode]++;
    rm.totalRequests++;
    rm.totalDurationMs += durationMs;

    // Bucket placement
    size_t bucketIdx = latencyBounds_.size(); // defaults to +Inf
    for (size_t i = 0; i < latencyBounds_.size(); ++i) {
        if (durationMs <= latencyBounds_[i]) {
            bucketIdx = i;
            break;
        }
    }
    rm.bucketCounts[bucketIdx]++;
}

void OtelMetrics::recordDbQuery(const std::string& operation, double durationMs, bool success) {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    dbMetrics_.operationCounts[operation]++;
    if (success) {
        dbMetrics_.totalSuccess++;
    } else {
        dbMetrics_.totalFailure++;
    }
    dbMetrics_.totalDurationMs += durationMs;
}

std::string OtelMetrics::toPrometheusText() const {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    std::ostringstream ss;

    // 1. HTTP Requests Total
    ss << "# HELP http_requests_total Total number of HTTP requests processed\n";
    ss << "# TYPE http_requests_total counter\n";
    for (const auto& [_, rm] : httpMetricsMap_) {
        for (const auto& [status, count] : rm.statusCounts) {
            ss << "http_requests_total{method=\"" << rm.method 
               << "\",route=\"" << rm.route 
               << "\",status=\"" << status << "\"} " << count << "\n";
        }
    }

    // 2. HTTP Request Duration Histogram
    ss << "\n# HELP http_request_duration_ms Latency of HTTP requests in milliseconds\n";
    ss << "# TYPE http_request_duration_ms histogram\n";
    for (const auto& [_, rm] : httpMetricsMap_) {
        uint64_t cumulative = 0;
        for (size_t i = 0; i < latencyBounds_.size(); ++i) {
            cumulative += rm.bucketCounts[i];
            ss << "http_request_duration_ms_bucket{method=\"" << rm.method 
               << "\",route=\"" << rm.route 
               << "\",le=\"" << latencyBounds_[i] << "\"} " << cumulative << "\n";
        }
        cumulative += rm.bucketCounts.back();
        ss << "http_request_duration_ms_bucket{method=\"" << rm.method 
           << "\",route=\"" << rm.route 
           << "\",le=\"+Inf\"} " << cumulative << "\n";
        ss << "http_request_duration_ms_sum{method=\"" << rm.method 
           << "\",route=\"" << rm.route << "\"} " << std::fixed << std::setprecision(2) << rm.totalDurationMs << "\n";
        ss << "http_request_duration_ms_count{method=\"" << rm.method 
           << "\",route=\"" << rm.route << "\"} " << rm.totalRequests << "\n";
    }

    // 3. Database Queries Total
    ss << "\n# HELP db_queries_total Total number of database operations executed\n";
    ss << "# TYPE db_queries_total counter\n";
    for (const auto& [op, count] : dbMetrics_.operationCounts) {
        ss << "db_queries_total{operation=\"" << op << "\",status=\"executed\"} " << count << "\n";
    }
    ss << "db_queries_total{status=\"success\"} " << dbMetrics_.totalSuccess.load() << "\n";
    ss << "db_queries_total{status=\"failure\"} " << dbMetrics_.totalFailure.load() << "\n";

    return ss.str();
}

std::string OtelMetrics::toOtlpJson(const std::string& serviceName, const std::string& serviceVersion) const {
    std::lock_guard<std::mutex> lock(metricsMutex_);

    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    auto now = std::chrono::system_clock::now();
    uint64_t nowNano = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());
    uint64_t startNano = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(startTime_.time_since_epoch()).count());

    writer.StartObject();
    writer.Key("resourceMetrics");
    writer.StartArray();

    writer.StartObject();
    // Resource
    writer.Key("resource");
    writer.StartObject();
    writer.Key("attributes");
    writer.StartArray();
    
    writer.StartObject();
    writer.Key("key"); writer.String("service.name");
    writer.Key("value"); writer.StartObject(); writer.Key("stringValue"); writer.String(serviceName.c_str()); writer.EndObject();
    writer.EndObject();

    writer.StartObject();
    writer.Key("key"); writer.String("service.version");
    writer.Key("value"); writer.StartObject(); writer.Key("stringValue"); writer.String(serviceVersion.c_str()); writer.EndObject();
    writer.EndObject();

    writer.EndArray();
    writer.EndObject(); // end resource

    // ScopeMetrics
    writer.Key("scopeMetrics");
    writer.StartArray();
    writer.StartObject();
    writer.Key("scope");
    writer.StartObject();
    writer.Key("name"); writer.String("drogon.http.server");
    writer.Key("version"); writer.String("1.0.0");
    writer.EndObject();

    writer.Key("metrics");
    writer.StartArray();

    // Metric: http.server.requests
    writer.StartObject();
    writer.Key("name"); writer.String("http.server.requests");
    writer.Key("description"); writer.String("Total number of HTTP requests");
    writer.Key("unit"); writer.String("1");
    writer.Key("sum");
    writer.StartObject();
    writer.Key("aggregationTemporality"); writer.Int(2); // CUMULATIVE
    writer.Key("isMonotonic"); writer.Bool(true);
    writer.Key("dataPoints");
    writer.StartArray();

    for (const auto& [_, rm] : httpMetricsMap_) {
        for (const auto& [status, count] : rm.statusCounts) {
            writer.StartObject();
            writer.Key("startTimeUnixNano"); writer.String(std::to_string(startNano).c_str());
            writer.Key("timeUnixNano"); writer.String(std::to_string(nowNano).c_str());
            writer.Key("asInt"); writer.String(std::to_string(count).c_str());
            writer.Key("attributes");
            writer.StartArray();

            writer.StartObject();
            writer.Key("key"); writer.String("http.method");
            writer.Key("value"); writer.StartObject(); writer.Key("stringValue"); writer.String(rm.method.c_str()); writer.EndObject();
            writer.EndObject();

            writer.StartObject();
            writer.Key("key"); writer.String("http.route");
            writer.Key("value"); writer.StartObject(); writer.Key("stringValue"); writer.String(rm.route.c_str()); writer.EndObject();
            writer.EndObject();

            writer.StartObject();
            writer.Key("key"); writer.String("http.status_code");
            writer.Key("value"); writer.StartObject(); writer.Key("intValue"); writer.String(std::to_string(status).c_str()); writer.EndObject();
            writer.EndObject();

            writer.EndArray();
            writer.EndObject();
        }
    }
    writer.EndArray(); // end dataPoints
    writer.EndObject(); // end sum
    writer.EndObject(); // end metric http.server.requests

    writer.EndArray(); // end metrics
    writer.EndObject(); // end scope object
    writer.EndArray(); // end scopeMetrics

    writer.EndObject(); // end resourceMetrics item
    writer.EndArray(); // end resourceMetrics
    writer.EndObject(); // end root

    return sb.GetString();
}

} // namespace api::otel
