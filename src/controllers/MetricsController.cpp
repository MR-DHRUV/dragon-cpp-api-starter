#include "api/controllers/MetricsController.h"
#include "api/otel/OtelMetrics.h"

namespace api::controllers {

void MetricsController::getMetrics(
    const drogon::HttpRequestPtr& /*req*/,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    std::string text = otel::OtelMetrics::instance().toPrometheusText();
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k200OK);
    resp->setContentTypeCode(drogon::CT_TEXT_PLAIN);
    resp->addHeader("Content-Type", "text/plain; version=0.0.4; charset=utf-8");
    resp->setBody(std::move(text));
    callback(resp);
}

} // namespace api::controllers
