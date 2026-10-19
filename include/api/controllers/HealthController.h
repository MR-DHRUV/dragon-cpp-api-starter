#pragma once

#include <drogon/HttpController.h>

namespace api::controllers {

class HealthController : public drogon::HttpController<HealthController> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(HealthController::healthCheck, "/api/v1/health", drogon::Get);
        ADD_METHOD_TO(HealthController::ping, "/api/v1/ping", drogon::Get);
    METHOD_LIST_END

    void healthCheck(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void ping(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

} // namespace api::controllers
