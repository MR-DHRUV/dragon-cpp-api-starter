#include "api/common/RapidJsonHelper.h"
#include "api/models/User.h"
#include "api/otel/OtelSpan.h"
#include "api/otel/OtelMetrics.h"
#include <iostream>
#include <cassert>

void testJsonHelperParsing() {
    std::string json = R"({"name":"John Doe","age":30,"active":true,"score":98.5})";
    std::string err;
    auto doc = api::common::RapidJsonHelper::parse(json, &err);
    assert(doc.has_value());
    assert(doc->IsObject());

    auto name = api::common::RapidJsonHelper::getString(*doc, "name");
    assert(name && *name == "John Doe");

    auto age = api::common::RapidJsonHelper::getInt(*doc, "age");
    assert(age && *age == 30);

    auto active = api::common::RapidJsonHelper::getBool(*doc, "active");
    assert(active && *active == true);

    auto score = api::common::RapidJsonHelper::getDouble(*doc, "score");
    assert(score && *score > 98.4 && *score < 98.6);

    std::cout << "[PASS] RapidJsonHelper parsing tests\n";
}

void testUserSerialization() {
    api::models::User user;
    user.id = 42;
    user.username = "turbo_dev";
    user.email = "turbo@speed.io";
    user.fullName = "Turbo Developer";
    user.isActive = true;
    user.createdAt = "2026-10-18T22:00:00Z";
    user.updatedAt = "2026-10-18T22:00:00Z";

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    user.serialize(writer);

    std::string output = buffer.GetString();
    assert(output.find("turbo_dev") != std::string::npos);
    assert(output.find("42") != std::string::npos);

    std::cout << "[PASS] User streaming serialization tests\n";
}

void testUsernameValidation() {
    std::string reason;
    assert(api::models::isValidUsername("john_doe", &reason));
    assert(api::models::isValidUsername("user123", &reason));
    assert(api::models::isValidUsername("abc", &reason)); // minimum 3 chars

    assert(!api::models::isValidUsername("", &reason)); // empty
    assert(!api::models::isValidUsername("ab", &reason)); // too short (<3)
    assert(!api::models::isValidUsername("this_username_is_way_too_long_to_be_allowed_1234567890", &reason)); // >30 chars
    assert(!api::models::isValidUsername("john-doe", &reason)); // hyphens not allowed (only alphanumeric + underscore)
    assert(!api::models::isValidUsername("user@name", &reason)); // invalid char @
    assert(!api::models::isValidUsername("user name", &reason)); // space not allowed

    std::cout << "[PASS] Username validation tests\n";
}

void testOtelTraceContext() {
    std::string traceId = api::otel::TraceIdGenerator::generateTraceId();
    std::string spanId = api::otel::TraceIdGenerator::generateSpanId();
    assert(traceId.length() == 32);
    assert(spanId.length() == 16);

    std::string traceParent = api::otel::TraceIdGenerator::formatTraceParent(traceId, spanId);
    assert(traceParent.rfind("00-" + traceId + "-" + spanId + "-01", 0) == 0);

    std::string parsedTrace;
    std::string parsedParent;
    assert(api::otel::TraceIdGenerator::parseTraceParent(traceParent, parsedTrace, parsedParent));
    assert(parsedTrace == traceId);
    assert(parsedParent == spanId);

    // Invalid format test
    assert(!api::otel::TraceIdGenerator::parseTraceParent("invalid-header", parsedTrace, parsedParent));

    std::cout << "[PASS] OpenTelemetry W3C TraceContext tests\n";
}

void testOtelMetrics() {
    auto& metrics = api::otel::OtelMetrics::instance();
    metrics.recordHttpRequest("GET", "/api/v1/ping", 200, 1.25);
    metrics.recordHttpRequest("POST", "/api/v1/users", 201, 45.0);
    metrics.recordDbQuery("SELECT 1", 0.5, true);

    std::string promText = metrics.toPrometheusText();
    assert(promText.find("http_requests_total{method=\"GET\",route=\"/api/v1/ping\",status=\"200\"} 1") != std::string::npos);
    assert(promText.find("http_request_duration_ms_bucket") != std::string::npos);
    assert(promText.find("db_queries_total{operation=\"SELECT 1\",status=\"executed\"} 1") != std::string::npos);

    std::string otlpJson = metrics.toOtlpJson("backend-server", "1.0.0");
    assert(otlpJson.find("resourceMetrics") != std::string::npos);
    assert(otlpJson.find("http.server.requests") != std::string::npos);

    std::cout << "[PASS] OpenTelemetry metrics aggregation tests\n";
}

int main() {
    std::cout << "Running Unit Tests...\n";
    testJsonHelperParsing();
    testUserSerialization();
    testUsernameValidation();
    testOtelTraceContext();
    testOtelMetrics();
    std::cout << "All unit tests completed successfully!\n";
    return 0;
}
