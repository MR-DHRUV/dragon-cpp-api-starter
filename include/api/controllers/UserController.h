#pragma once

#include <drogon/HttpController.h>

namespace api::controllers {

class UserController : public drogon::HttpController<UserController> {
public:
    METHOD_LIST_BEGIN
        ADD_METHOD_TO(UserController::getUsers, "/api/v1/users", drogon::Get);
        ADD_METHOD_TO(UserController::checkUsernameQuery, "/api/v1/users/check-username", drogon::Get);
        ADD_METHOD_TO(UserController::checkUsernameAvailability, "/api/v1/users/availability/{1}", drogon::Get);
        ADD_METHOD_TO(UserController::getUserById, "/api/v1/users/{1}", drogon::Get);
        ADD_METHOD_TO(UserController::createUser, "/api/v1/users", drogon::Post);
        ADD_METHOD_TO(UserController::updateUser, "/api/v1/users/{1}", drogon::Put);
        ADD_METHOD_TO(UserController::deleteUser, "/api/v1/users/{1}", drogon::Delete);
    METHOD_LIST_END

    void checkUsernameAvailability(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        const std::string& username);

    void checkUsernameQuery(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void getUsers(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void getUserById(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        int64_t id);

    void createUser(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void updateUser(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        int64_t id);

    void deleteUser(
        const drogon::HttpRequestPtr& req,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        int64_t id);
};

} // namespace api::controllers
