#pragma once

#include <drogon/HttpController.h>

namespace api::controllers {

class MetricsController : public drogon::HttpController<MetricsController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(MetricsController::getMetrics, "/api/v1/metrics", drogon::Get);
    METHOD_LIST_END

    void getMetrics(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );
};

} // namespace api::controllers
