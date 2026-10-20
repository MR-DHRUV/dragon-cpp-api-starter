#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>

namespace api::otel {

struct OtelLogRecord {
    uint64_t timeUnixNano{0};
    int severityNumber{9};       // 9=INFO, 13=WARN, 17=ERROR, 21=FATAL
    std::string severityText;    // "INFO", "WARN", etc.
    std::string body;
    std::string traceId;
    std::string spanId;
    std::vector<std::pair<std::string, std::string>> attributes;
};

class OtelLogger {
public:
    static void initialize();
    static void handleLog(const char* msg, uint64_t len);
};

} // namespace api::otel
