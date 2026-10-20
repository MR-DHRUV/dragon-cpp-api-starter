#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <random>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace api::otel {

enum class SpanKind : int {
    Internal = 1,
    Server = 2,
    Client = 3,
    Producer = 4,
    Consumer = 5
};

enum class StatusCode : int {
    Unset = 0,
    Ok = 1,
    Error = 2
};

struct TraceContext {
    std::string traceId;
    std::string spanId;
    bool isValid() const {
        return traceId.length() == 32 && spanId.length() == 16;
    }
};

/**
 * @brief Thread-local RAII guard for the active OpenTelemetry TraceContext.
 */
class TraceContextScope {
public:
    explicit TraceContextScope(const TraceContext& ctx);
    ~TraceContextScope();

    static TraceContext current();
    static void setCurrent(const TraceContext& ctx);
    static void clear();

private:
    TraceContext previous_;
};

/**
 * @brief Generator and parser for W3C TraceContext (traceparent).
 */
class TraceIdGenerator {
public:
    static std::string generateTraceId();
    static std::string generateSpanId();
    static bool parseTraceParent(const std::string& header, std::string& outTraceId, std::string& outParentSpanId);
    static std::string formatTraceParent(const std::string& traceId, const std::string& spanId);
};

/**
 * @brief Representation of an individual OpenTelemetry Span.
 */
class OtelSpan {
public:
    OtelSpan(std::string name, SpanKind kind, std::string traceId, std::string spanId, std::string parentSpanId = "");

    void setAttribute(std::string key, std::string value);
    void setAttribute(std::string key, int64_t value);
    void setAttribute(std::string key, double value);
    void setAttribute(std::string key, bool value);

    void setStatus(StatusCode code, std::string description = "");
    void end();

    const std::string& getName() const { return name_; }
    SpanKind getKind() const { return kind_; }
    const std::string& getTraceId() const { return traceId_; }
    const std::string& getSpanId() const { return spanId_; }
    const std::string& getParentSpanId() const { return parentSpanId_; }
    uint64_t getStartTimeUnixNano() const { return startTimeUnixNano_; }
    uint64_t getEndTimeUnixNano() const { return endTimeUnixNano_; }
    StatusCode getStatusCode() const { return statusCode_; }
    const std::string& getStatusMessage() const { return statusMessage_; }
    const std::vector<std::pair<std::string, std::string>>& getAttributes() const { return attributes_; }
    bool isEnded() const { return ended_; }

private:
    std::string name_;
    SpanKind kind_;
    std::string traceId_;
    std::string spanId_;
    std::string parentSpanId_;
    uint64_t startTimeUnixNano_{0};
    uint64_t endTimeUnixNano_{0};
    StatusCode statusCode_{StatusCode::Ok};
    std::string statusMessage_;
    std::vector<std::pair<std::string, std::string>> attributes_;
    bool ended_{false};
};

using OtelSpanPtr = std::shared_ptr<OtelSpan>;

} // namespace api::otel
