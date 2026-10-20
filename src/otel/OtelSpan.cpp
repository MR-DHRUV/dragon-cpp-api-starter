#include "api/otel/OtelSpan.h"

namespace api::otel {

thread_local TraceContext g_currentTraceContext;

TraceContextScope::TraceContextScope(const TraceContext& ctx)
    : previous_(g_currentTraceContext) 
{
    g_currentTraceContext = ctx;
}

TraceContextScope::~TraceContextScope() {
    g_currentTraceContext = previous_;
}

TraceContext TraceContextScope::current() {
    return g_currentTraceContext;
}

void TraceContextScope::setCurrent(const TraceContext& ctx) {
    g_currentTraceContext = ctx;
}

void TraceContextScope::clear() {
    g_currentTraceContext = TraceContext{};
}

std::string TraceIdGenerator::generateTraceId() {
    thread_local std::random_device rd;
    thread_local std::mt19937_64 gen(rd());
    thread_local std::uniform_int_distribution<uint64_t> dis;

    uint64_t part1 = dis(gen);
    uint64_t part2 = dis(gen);

    std::ostringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << part1
       << std::setw(16) << part2;
    return ss.str();
}

std::string TraceIdGenerator::generateSpanId() {
    thread_local std::random_device rd;
    thread_local std::mt19937_64 gen(rd());
    thread_local std::uniform_int_distribution<uint64_t> dis;

    uint64_t part = dis(gen);
    std::ostringstream ss;
    ss << std::hex << std::setfill('0') << std::setw(16) << part;
    return ss.str();
}

bool TraceIdGenerator::parseTraceParent(
    const std::string& header,
    std::string& outTraceId,
    std::string& outParentSpanId) 
{
    // Format: 00-4bf92f3577b34da6a3ce929d0e0e4736-00f067aa0ba902b7-01
    if (header.length() < 55) {
        return false;
    }
    if (header[2] != '-' || header[35] != '-' || header[52] != '-') {
        return false;
    }

    std::string version = header.substr(0, 2);
    if (version != "00") {
        return false;
    }

    outTraceId = header.substr(3, 32);
    outParentSpanId = header.substr(36, 16);

    // Verify all characters are hexadecimal
    auto isHex = [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    };

    if (!std::all_of(outTraceId.begin(), outTraceId.end(), isHex) ||
        !std::all_of(outParentSpanId.begin(), outParentSpanId.end(), isHex)) {
        return false;
    }

    return true;
}

std::string TraceIdGenerator::formatTraceParent(const std::string& traceId, const std::string& spanId) {
    return "00-" + traceId + "-" + spanId + "-01";
}

OtelSpan::OtelSpan(
    std::string name,
    SpanKind kind,
    std::string traceId,
    std::string spanId,
    std::string parentSpanId)
    : name_(std::move(name))
    , kind_(kind)
    , traceId_(std::move(traceId))
    , spanId_(std::move(spanId))
    , parentSpanId_(std::move(parentSpanId))
{
    auto now = std::chrono::system_clock::now();
    startTimeUnixNano_ = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());
}

void OtelSpan::setAttribute(std::string key, std::string value) {
    attributes_.emplace_back(std::move(key), std::move(value));
}

void OtelSpan::setAttribute(std::string key, int64_t value) {
    attributes_.emplace_back(std::move(key), std::to_string(value));
}

void OtelSpan::setAttribute(std::string key, double value) {
    attributes_.emplace_back(std::move(key), std::to_string(value));
}

void OtelSpan::setAttribute(std::string key, bool value) {
    attributes_.emplace_back(std::move(key), value ? "true" : "false");
}

void OtelSpan::setStatus(StatusCode code, std::string description) {
    statusCode_ = code;
    statusMessage_ = std::move(description);
}

void OtelSpan::end() {
    if (!ended_) {
        auto now = std::chrono::system_clock::now();
        endTimeUnixNano_ = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());
        ended_ = true;
    }
}

} // namespace api::otel
