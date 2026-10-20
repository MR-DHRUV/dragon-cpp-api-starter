#define _CRT_SECURE_NO_WARNINGS
#include "api/otel/OtelManager.h"
#include <drogon/drogon.h>
#include <trantor/utils/Logger.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <iostream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

namespace api::otel {

void OtelLogger::initialize() {
    trantor::Logger::setOutputFunction(
        [](const char* msg, const uint64_t len) {
            handleLog(msg, len);
        },
        []() {
            std::cout.flush();
        }
    );
}

void OtelLogger::handleLog(const char* msg, uint64_t len) {
    if (!msg || len == 0) return;

    std::string str(msg, len);

    // Print to standard console
    std::cout << str;

    if (!OtelManager::instance().isEnabled()) {
        return;
    }

    // Determine severity
    int severityNumber = 9; // INFO
    std::string severityText = "INFO";
    if (str.find("TRACE") != std::string::npos) {
        severityNumber = 1; severityText = "TRACE";
    } else if (str.find("DEBUG") != std::string::npos) {
        severityNumber = 5; severityText = "DEBUG";
    } else if (str.find("WARN") != std::string::npos) {
        severityNumber = 13; severityText = "WARN";
    } else if (str.find("ERROR") != std::string::npos) {
        severityNumber = 17; severityText = "ERROR";
    } else if (str.find("FATAL") != std::string::npos) {
        severityNumber = 21; severityText = "FATAL";
    }

    // Strip trailing newline if any for body
    while (!str.empty() && (str.back() == '\n' || str.back() == '\r')) {
        str.pop_back();
    }

    auto now = std::chrono::system_clock::now();
    uint64_t nowNano = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());

    TraceContext ctx = TraceContextScope::current();

    OtelLogRecord rec;
    rec.timeUnixNano = nowNano;
    rec.severityNumber = severityNumber;
    rec.severityText = severityText;
    rec.body = std::move(str);
    rec.traceId = ctx.traceId;
    rec.spanId = ctx.spanId;

    OtelManager::instance().enqueueLog(rec);
}

OtelManager::~OtelManager() {
    shutdown();
}

void OtelManager::initialize(const Json::Value& configRoot) {
    if (configRoot.isMember("opentelemetry")) {
        const auto& otel = configRoot["opentelemetry"];
        config_.enabled = otel.get("enabled", true).asBool();
        config_.serviceName = otel.get("service_name", "backend-server").asString();
        config_.serviceVersion = otel.get("service_version", "1.0.0").asString();
        config_.collectorEndpoint = otel.get("collector_http_endpoint", "http://127.0.0.1:4318").asString();
        config_.batchTimeoutMs = otel.get("batch_timeout_ms", 1000).asInt();
        config_.maxQueueSize = otel.get("max_queue_size", 4096).asUInt();
        config_.exportTraces = otel.get("export_traces", true).asBool();
        config_.exportMetrics = otel.get("export_metrics", true).asBool();
        config_.exportLogs = otel.get("export_logs", true).asBool();
    }

    // Check environment overrides
    const char* envEndpoint = std::getenv("OTEL_EXPORTER_OTLP_ENDPOINT");
    if (envEndpoint && envEndpoint[0] != '\0') {
        config_.collectorEndpoint = envEndpoint;
    }

    const char* envServiceName = std::getenv("OTEL_SERVICE_NAME");
    if (envServiceName && envServiceName[0] != '\0') {
        config_.serviceName = envServiceName;
    }

    if (!config_.enabled) {
        LOG_INFO << "OpenTelemetry is disabled via configuration.";
        return;
    }

    OtelLogger::initialize();

    running_ = true;
    lastMetricsFlush_ = std::chrono::steady_clock::now();
    exporterThread_ = std::thread(&OtelManager::exporterWorkerLoop, this);

    LOG_INFO << "[OpenTelemetry] Initialized for service: " << config_.serviceName
             << " (Version: " << config_.serviceVersion 
             << ") -> OTLP Endpoint: " << config_.collectorEndpoint;
}

// ScopedSpan implementation
ScopedSpan::ScopedSpan(
    const std::string& name,
    const SpanPtr& parent,
    SpanKind kind)
{
    std::string traceId;
    std::string parentSpanId;

    if (parent && parent->underlyingSpan()) {
        traceId = parent->traceId();
        parentSpanId = parent->spanId();
    } else {
        traceId = TraceIdGenerator::generateTraceId();
        parentSpanId = "";
    }

    std::string spanId = TraceIdGenerator::generateSpanId();
    span_ = std::make_shared<OtelSpan>(name, kind, std::move(traceId), std::move(spanId), std::move(parentSpanId));
}

ScopedSpan::ScopedSpan(
    const std::string& name,
    const std::string& traceId,
    const std::string& parentSpanId,
    SpanKind kind)
{
    std::string tId = traceId.empty() ? TraceIdGenerator::generateTraceId() : traceId;
    std::string spanId = TraceIdGenerator::generateSpanId();
    span_ = std::make_shared<OtelSpan>(name, kind, std::move(tId), std::move(spanId), parentSpanId);
}

ScopedSpan& ScopedSpan::attr(std::string key, std::string value) {
    if (span_) span_->setAttribute(std::move(key), std::move(value));
    return *this;
}

ScopedSpan& ScopedSpan::attr(std::string key, const char* value) {
    if (span_ && value) span_->setAttribute(std::move(key), std::string(value));
    return *this;
}

ScopedSpan& ScopedSpan::attr(std::string key, int64_t value) {
    if (span_) span_->setAttribute(std::move(key), value);
    return *this;
}

ScopedSpan& ScopedSpan::attr(std::string key, int value) {
    if (span_) span_->setAttribute(std::move(key), static_cast<int64_t>(value));
    return *this;
}

ScopedSpan& ScopedSpan::attr(std::string key, size_t value) {
    if (span_) span_->setAttribute(std::move(key), static_cast<int64_t>(value));
    return *this;
}

ScopedSpan& ScopedSpan::attr(std::string key, double value) {
    if (span_) span_->setAttribute(std::move(key), value);
    return *this;
}

ScopedSpan& ScopedSpan::attr(std::string key, bool value) {
    if (span_) span_->setAttribute(std::move(key), value);
    return *this;
}

void ScopedSpan::fail(const std::string& msg) {
    if (span_) {
        span_->setStatus(StatusCode::Error, msg);
    }
}

void ScopedSpan::setStatus(StatusCode code, const std::string& description) {
    if (span_) {
        span_->setStatus(code, description);
    }
}

void ScopedSpan::end() {
    if (!ended_.exchange(true)) {
        if (span_) {
            span_->end();
            OtelManager::instance().enqueueSpan(span_);
        }
    }
}

const std::string& ScopedSpan::traceId() const {
    static const std::string empty;
    return span_ ? span_->getTraceId() : empty;
}

const std::string& ScopedSpan::spanId() const {
    static const std::string empty;
    return span_ ? span_->getSpanId() : empty;
}

void OtelManager::setupDrogonHooks() {
    if (!config_.enabled) return;

    // 1. PreRouting: Create root Server span once and store in req->attributes() as "otel.span"
    drogon::app().registerPreRoutingAdvice([](const drogon::HttpRequestPtr& req) {
        std::string traceId;
        std::string parentSpanId;
        std::string traceParent = req->getHeader("traceparent");

        if (traceParent.empty() || !TraceIdGenerator::parseTraceParent(traceParent, traceId, parentSpanId)) {
            traceId = TraceIdGenerator::generateTraceId();
            parentSpanId = "";
        }

        std::string methodStr = req->getMethodString();
        std::string spanName = methodStr + " " + req->getPath();
        auto rootSpan = std::make_shared<ScopedSpan>(spanName, traceId, parentSpanId, SpanKind::Server);
        rootSpan->attr("http.method", methodStr);
        rootSpan->attr("http.route", req->getPath());
        std::string target = req->getPath() + (req->getQuery().empty() ? "" : "?" + req->getQuery());
        rootSpan->attr("http.target", target);
        rootSpan->attr("http.client_ip", req->getPeerAddr().toIp());
        
        std::string userAgent = req->getHeader("user-agent");
        if (!userAgent.empty()) {
            rootSpan->attr("http.user_agent", userAgent);
        }

        req->getAttributes()->insert("otel.span", rootSpan);

        TraceContext ctx{rootSpan->traceId(), rootSpan->spanId()};
        TraceContextScope::setCurrent(ctx);
    });

    // 2. PreSending: Set http.status_code, end(), record metrics, and inject traceparent header
    drogon::app().registerPreSendingAdvice([](const drogon::HttpRequestPtr& req, const drogon::HttpResponsePtr& resp) {
        if (!resp) return;
        auto rootSpan = req->getAttributes()->get<SpanPtr>("otel.span");
        if (rootSpan) {
            int statusCode = resp->getStatusCode();
            rootSpan->attr("http.status_code", static_cast<int64_t>(statusCode));
            
            if (statusCode >= 500) {
                rootSpan->fail("Internal Server Error (" + std::to_string(statusCode) + ")");
            } else {
                rootSpan->setStatus(StatusCode::Ok);
            }
            rootSpan->end();

            double durationMs = 0.0;
            if (rootSpan->underlyingSpan()) {
                durationMs = static_cast<double>(rootSpan->underlyingSpan()->getEndTimeUnixNano() - rootSpan->underlyingSpan()->getStartTimeUnixNano()) / 1000000.0;
            }
            OtelMetrics::instance().recordHttpRequest(req->getMethodString(), req->getPath(), statusCode, durationMs);

            resp->addHeader("traceparent", TraceIdGenerator::formatTraceParent(rootSpan->traceId(), rootSpan->spanId()));
        }

        TraceContextScope::clear();
    });

    LOG_INFO << "[OpenTelemetry] Drogon PreRouting and PreSending advices registered.";
}

OtelSpanPtr OtelManager::startChildSpan(const std::string& name, SpanKind kind) {
    return startChildSpan(name, nullptr, kind);
}

OtelSpanPtr OtelManager::startChildSpan(const std::string& name, const OtelSpanPtr& parentSpan, SpanKind kind) {
    if (!config_.enabled) return nullptr;

    std::string traceId;
    std::string parentSpanId;

    if (parentSpan) {
        traceId = parentSpan->getTraceId();
        parentSpanId = parentSpan->getSpanId();
    } else {
        TraceContext parentCtx = TraceContextScope::current();
        traceId = parentCtx.isValid() ? parentCtx.traceId : TraceIdGenerator::generateTraceId();
        parentSpanId = parentCtx.isValid() ? parentCtx.spanId : "";
    }

    std::string spanId = TraceIdGenerator::generateSpanId();
    return std::make_shared<OtelSpan>(name, kind, traceId, spanId, parentSpanId);
}

void OtelManager::enqueueSpan(OtelSpanPtr span) {
    if (!config_.enabled || !config_.exportTraces || !span) return;

    std::unique_lock<std::mutex> lock(spanQueueMutex_);
    if (spanQueue_.size() >= config_.maxQueueSize) {
        spanQueue_.pop_front(); // Evict oldest
    }
    spanQueue_.push_back(std::move(span));
    spanCv_.notify_one();
}

void OtelManager::enqueueLog(const OtelLogRecord& log) {
    if (!config_.enabled || !config_.exportLogs) return;

    std::unique_lock<std::mutex> lock(logQueueMutex_);
    if (logQueue_.size() >= config_.maxQueueSize) {
        logQueue_.pop_front(); // Evict oldest
    }
    logQueue_.push_back(log);
}

void OtelManager::shutdown() {
    if (running_.exchange(false)) {
        spanCv_.notify_all();
        if (exporterThread_.joinable()) {
            exporterThread_.join();
        }
    }
}

void OtelManager::exporterWorkerLoop() {
    while (running_) {
        std::vector<OtelSpanPtr> spansToFlush;
        std::vector<OtelLogRecord> logsToFlush;

        {
            std::unique_lock<std::mutex> lock(spanQueueMutex_);
            spanCv_.wait_for(lock, std::chrono::milliseconds(config_.batchTimeoutMs), [this]() {
                return !running_ || !spanQueue_.empty();
            });

            if (!spanQueue_.empty()) {
                spansToFlush.reserve(spanQueue_.size());
                while (!spanQueue_.empty()) {
                    spansToFlush.push_back(std::move(spanQueue_.front()));
                    spanQueue_.pop_front();
                }
            }
        }

        {
            std::unique_lock<std::mutex> lock(logQueueMutex_);
            if (!logQueue_.empty()) {
                logsToFlush.reserve(logQueue_.size());
                while (!logQueue_.empty()) {
                    logsToFlush.push_back(std::move(logQueue_.front()));
                    logQueue_.pop_front();
                }
            }
        }

        // Flush spans
        if (!spansToFlush.empty() && config_.exportTraces) {
            flushSpans(spansToFlush);
        }

        // Flush logs
        if (!logsToFlush.empty() && config_.exportLogs) {
            flushLogs(logsToFlush);
        }

        // Flush metrics periodically (every batchTimeoutMs)
        auto now = std::chrono::steady_clock::now();
        if (config_.exportMetrics && 
            std::chrono::duration_cast<std::chrono::milliseconds>(now - lastMetricsFlush_).count() >= config_.batchTimeoutMs) {
            flushMetrics();
            lastMetricsFlush_ = now;
        }
    }
}

void OtelManager::flushSpans(std::vector<OtelSpanPtr>& spansToExport) {
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    writer.Key("resourceSpans");
    writer.StartArray();

    writer.StartObject();
    // Resource
    writer.Key("resource");
    writer.StartObject();
    writer.Key("attributes");
    writer.StartArray();

    writer.StartObject();
    writer.Key("key"); writer.String("service.name");
    writer.Key("value"); writer.StartObject(); writer.Key("stringValue"); writer.String(config_.serviceName.c_str()); writer.EndObject();
    writer.EndObject();

    writer.StartObject();
    writer.Key("key"); writer.String("service.version");
    writer.Key("value"); writer.StartObject(); writer.Key("stringValue"); writer.String(config_.serviceVersion.c_str()); writer.EndObject();
    writer.EndObject();

    writer.EndArray();
    writer.EndObject(); // end resource

    // ScopeSpans
    writer.Key("scopeSpans");
    writer.StartArray();
    writer.StartObject();
    writer.Key("scope");
    writer.StartObject();
    writer.Key("name"); writer.String("drogon.http.server");
    writer.Key("version"); writer.String("1.0.0");
    writer.EndObject();

    writer.Key("spans");
    writer.StartArray();

    for (const auto& span : spansToExport) {
        if (!span) continue;
        writer.StartObject();
        writer.Key("traceId"); writer.String(span->getTraceId().c_str());
        writer.Key("spanId"); writer.String(span->getSpanId().c_str());
        if (!span->getParentSpanId().empty()) {
            writer.Key("parentSpanId"); writer.String(span->getParentSpanId().c_str());
        }
        writer.Key("name"); writer.String(span->getName().c_str());
        writer.Key("kind"); writer.Int(static_cast<int>(span->getKind()));
        writer.Key("startTimeUnixNano"); writer.String(std::to_string(span->getStartTimeUnixNano()).c_str());
        writer.Key("endTimeUnixNano"); writer.String(std::to_string(span->getEndTimeUnixNano()).c_str());

        writer.Key("attributes");
        writer.StartArray();
        for (const auto& [k, v] : span->getAttributes()) {
            writer.StartObject();
            writer.Key("key"); writer.String(k.c_str());
            writer.Key("value"); writer.StartObject(); writer.Key("stringValue"); writer.String(v.c_str()); writer.EndObject();
            writer.EndObject();
        }
        writer.EndArray();

        writer.Key("status");
        writer.StartObject();
        writer.Key("code"); writer.Int(static_cast<int>(span->getStatusCode()));
        if (!span->getStatusMessage().empty()) {
            writer.Key("message"); writer.String(span->getStatusMessage().c_str());
        }
        writer.EndObject();

        writer.EndObject();
    }

    writer.EndArray(); // end spans
    writer.EndObject(); // end scope object
    writer.EndArray(); // end scopeSpans

    writer.EndObject(); // end resourceSpans item
    writer.EndArray(); // end resourceSpans
    writer.EndObject(); // end root

    sendHttpJson("/v1/traces", sb.GetString());
}

void OtelManager::flushLogs(std::vector<OtelLogRecord>& logsToExport) {
    rapidjson::StringBuffer sb;
    rapidjson::Writer<rapidjson::StringBuffer> writer(sb);

    writer.StartObject();
    writer.Key("resourceLogs");
    writer.StartArray();

    writer.StartObject();
    // Resource
    writer.Key("resource");
    writer.StartObject();
    writer.Key("attributes");
    writer.StartArray();

    writer.StartObject();
    writer.Key("key"); writer.String("service.name");
    writer.Key("value"); writer.StartObject(); writer.Key("stringValue"); writer.String(config_.serviceName.c_str()); writer.EndObject();
    writer.EndObject();

    writer.StartObject();
    writer.Key("key"); writer.String("service.version");
    writer.Key("value"); writer.StartObject(); writer.Key("stringValue"); writer.String(config_.serviceVersion.c_str()); writer.EndObject();
    writer.EndObject();

    writer.EndArray();
    writer.EndObject(); // end resource

    // ScopeLogs
    writer.Key("scopeLogs");
    writer.StartArray();
    writer.StartObject();
    writer.Key("scope");
    writer.StartObject();
    writer.Key("name"); writer.String("drogon.logger");
    writer.Key("version"); writer.String("1.0.0");
    writer.EndObject();

    writer.Key("logRecords");
    writer.StartArray();

    for (const auto& log : logsToExport) {
        writer.StartObject();
        writer.Key("timeUnixNano"); writer.String(std::to_string(log.timeUnixNano).c_str());
        writer.Key("severityNumber"); writer.Int(log.severityNumber);
        writer.Key("severityText"); writer.String(log.severityText.c_str());
        writer.Key("body"); writer.StartObject(); writer.Key("stringValue"); writer.String(log.body.c_str()); writer.EndObject();
        if (!log.traceId.empty()) {
            writer.Key("traceId"); writer.String(log.traceId.c_str());
        }
        if (!log.spanId.empty()) {
            writer.Key("spanId"); writer.String(log.spanId.c_str());
        }
        writer.EndObject();
    }

    writer.EndArray(); // end logRecords
    writer.EndObject(); // end scope object
    writer.EndArray(); // end scopeLogs

    writer.EndObject(); // end resourceLogs item
    writer.EndArray(); // end resourceLogs
    writer.EndObject(); // end root

    sendHttpJson("/v1/logs", sb.GetString());
}

void OtelManager::flushMetrics() {
    std::string json = OtelMetrics::instance().toOtlpJson(config_.serviceName, config_.serviceVersion);
    sendHttpJson("/v1/metrics", json);
}

bool OtelManager::sendHttpJson(const std::string& urlPath, const std::string& jsonBody) {
#ifdef _WIN32
    // Parse host and port from config_.collectorEndpoint
    std::string endpoint = config_.collectorEndpoint;
    std::string scheme = "http";
    std::string host = "127.0.0.1";
    INTERNET_PORT port = 4318;

    size_t schemePos = endpoint.find("://");
    if (schemePos != std::string::npos) {
        scheme = endpoint.substr(0, schemePos);
        endpoint = endpoint.substr(schemePos + 3);
    }

    size_t colonPos = endpoint.find(':');
    if (colonPos != std::string::npos) {
        host = endpoint.substr(0, colonPos);
        port = static_cast<INTERNET_PORT>(std::stoi(endpoint.substr(colonPos + 1)));
    } else {
        host = endpoint;
    }

    std::wstring wHost(host.begin(), host.end());
    std::wstring wPath(urlPath.begin(), urlPath.end());

    HINTERNET hSession = WinHttpOpen(L"OtelManager/1.0",
                                     WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, wHost.c_str(), port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD dwFlags = (scheme == "https") ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wPath.c_str(),
                                           NULL, WINHTTP_NO_REFERER,
                                           WINHTTP_DEFAULT_ACCEPT_TYPES,
                                           dwFlags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Set 1-second timeout so worker thread never blocks
    WinHttpSetTimeouts(hRequest, 1000, 1000, 1000, 1000);

    LPCWSTR headers = L"Content-Type: application/json\r\n";
    BOOL bResults = WinHttpSendRequest(hRequest,
                                       headers, static_cast<DWORD>(-1L),
                                       (LPVOID)jsonBody.data(), static_cast<DWORD>(jsonBody.size()),
                                       static_cast<DWORD>(jsonBody.size()), 0);

    if (bResults) {
        WinHttpReceiveResponse(hRequest, NULL);
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return bResults == TRUE;
#else
    return false;
#endif
}

} // namespace api::otel
