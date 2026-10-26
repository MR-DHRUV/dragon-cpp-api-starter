#pragma once

#include <drogon/HttpController.h>

namespace api::controllers {

/**
 * @brief Controller that serves interactive Swagger UI documentation and OpenAPI 3.0 JSON specification.
 */
class DocsController : public drogon::HttpController<DocsController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(DocsController::getSwaggerUi, "/docs", drogon::Get);
    ADD_METHOD_TO(DocsController::getSwaggerUi, "/swagger", drogon::Get);
    ADD_METHOD_TO(DocsController::getOpenApiJson, "/api/v1/openapi.json", drogon::Get);
    ADD_METHOD_TO(DocsController::getOpenApiJson, "/openapi.json", drogon::Get);
    METHOD_LIST_END

    void getSwaggerUi(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );

    void getOpenApiJson(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback
    );
};

} // namespace api::controllers
