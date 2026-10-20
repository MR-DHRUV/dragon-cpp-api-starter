#pragma once

#include "OtelSpan.h"
#include <string>
#include <memory>
#include <atomic>
#include <utility>

namespace api::otel {

class ScopedSpan;
using SpanPtr = std::shared_ptr<ScopedSpan>;

class ScopedSpan {
public:
    ScopedSpan(
        const std::string& name,
        const SpanPtr& parent = nullptr,
        SpanKind kind = SpanKind::Internal);

    ScopedSpan(
        const std::string& name,
        const std::string& traceId,
        const std::string& parentSpanId,
        SpanKind kind = SpanKind::Server);

    ~ScopedSpan() {
        end();
    }

    ScopedSpan(const ScopedSpan&) = delete;
    ScopedSpan& operator=(const ScopedSpan&) = delete;

    ScopedSpan& attr(std::string key, std::string value);
    ScopedSpan& attr(std::string key, const char* value);
    ScopedSpan& attr(std::string key, int64_t value);
    ScopedSpan& attr(std::string key, int value);
    ScopedSpan& attr(std::string key, size_t value);
    ScopedSpan& attr(std::string key, double value);
    ScopedSpan& attr(std::string key, bool value);

    void fail(const std::string& msg);
    void setStatus(StatusCode code, const std::string& description = "");

    void end();

    const std::string& traceId() const;
    const std::string& spanId() const;
    const OtelSpanPtr& underlyingSpan() const { return span_; }
    bool isEnded() const { return ended_.load(); }

private:
    OtelSpanPtr span_;
    std::atomic<bool> ended_{false};
};

} // namespace api::otel
