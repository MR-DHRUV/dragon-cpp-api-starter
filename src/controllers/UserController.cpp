#include "api/controllers/UserController.h"
#include "api/services/UserService.h"
#include "api/common/ApiResponse.h"
#include "api/common/RapidJsonHelper.h"
#include "api/otel/ScopedSpan.h"

#include <charconv>

namespace api::controllers {

void UserController::checkUsernameAvailability(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    const std::string& username)
{
    auto span = req ? req->getAttributes()->get<otel::SpanPtr>("otel.span") : nullptr;

    services::UserService::instance().checkUsernameAvailability(
        username,
        [username, callback = std::move(callback)](bool queryOk, bool available, bool validFormat, const std::string& reason) {
            if (!queryOk) {
                callback(common::ApiResponse::error(drogon::k500InternalServerError, reason));
                return;
            }

            if (!validFormat) {
                auto resp = common::ApiResponse::successWithPayload([&username, &reason](rapidjson::Writer<rapidjson::StringBuffer>& writer) {
                    writer.StartObject();
                    writer.Key("username");
                    writer.String(username.c_str(), static_cast<rapidjson::SizeType>(username.length()));
                    writer.Key("available");
                    writer.Bool(false);
                    writer.Key("valid");
                    writer.Bool(false);
                    writer.Key("reason");
                    writer.String(reason.c_str(), static_cast<rapidjson::SizeType>(reason.length()));
                    writer.EndObject();
                }, reason, drogon::k400BadRequest);
                callback(resp);
                return;
            }

            auto resp = common::ApiResponse::successWithPayload([&username, available, &reason](rapidjson::Writer<rapidjson::StringBuffer>& writer) {
                writer.StartObject();
                writer.Key("username");
                writer.String(username.c_str(), static_cast<rapidjson::SizeType>(username.length()));
                writer.Key("available");
                writer.Bool(available);
                writer.Key("valid");
                writer.Bool(true);
                writer.Key("reason");
                writer.String(reason.c_str(), static_cast<rapidjson::SizeType>(reason.length()));
                writer.EndObject();
            }, available ? "Username is available" : "Username is already taken", drogon::k200OK);

            callback(resp);
        },
        span);
}

void UserController::checkUsernameQuery(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback)
{
    auto username = req->getParameter("username");
    if (username.empty()) {
        callback(common::ApiResponse::error(drogon::k400BadRequest, "Missing 'username' query parameter"));
        return;
    }
    checkUsernameAvailability(req, std::move(callback), username);
}

void UserController::getUsers(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) 
{
    size_t limit = 50;
    size_t offset = 0;

    auto limitParam = req->getParameter("limit");
    if (!limitParam.empty()) {
        size_t val = 0;
        if (auto [ptr, ec] = std::from_chars(limitParam.data(), limitParam.data() + limitParam.size(), val); ec == std::errc()) {
            limit = std::min<size_t>(val, 500); // capped at 500 for safety
        }
    }

    auto offsetParam = req->getParameter("offset");
    if (!offsetParam.empty()) {
        size_t val = 0;
        if (auto [ptr, ec] = std::from_chars(offsetParam.data(), offsetParam.data() + offsetParam.size(), val); ec == std::errc()) {
            offset = val;
        }
    }

    auto span = req ? req->getAttributes()->get<otel::SpanPtr>("otel.span") : nullptr;

    services::UserService::instance().getAllUsers(
        limit, offset,
        [callback = std::move(callback)](bool ok, const std::vector<models::User>& users, const std::string& error) {
            if (!ok) {
                callback(common::ApiResponse::error(drogon::k500InternalServerError, error));
                return;
            }

            auto resp = common::ApiResponse::successWithPayload([&users](rapidjson::Writer<rapidjson::StringBuffer>& writer) {
                writer.StartArray();
                for (const auto& u : users) {
                    u.serialize(writer);
                }
                writer.EndArray();
            }, "Users retrieved successfully");

            callback(resp);
        },
        span);
}

void UserController::getUserById(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    int64_t id) 
{
    auto span = req ? req->getAttributes()->get<otel::SpanPtr>("otel.span") : nullptr;

    services::UserService::instance().getUserById(
        id,
        [id, callback = std::move(callback)](bool ok, const std::optional<models::User>& user, const std::string& error) {
            if (!ok) {
                callback(common::ApiResponse::error(drogon::k500InternalServerError, error));
                return;
            }
            if (!user) {
                callback(common::ApiResponse::error(drogon::k404NotFound, "User not found with id " + std::to_string(id)));
                return;
            }

            auto resp = common::ApiResponse::successWithPayload([&user](rapidjson::Writer<rapidjson::StringBuffer>& writer) {
                user->serialize(writer);
            }, "User retrieved successfully");

            callback(resp);
        },
        span);
}

void UserController::createUser(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) 
{
    std::string errorMsg;
    auto dto = models::CreateUserDto::fromJson(req->getBody(), &errorMsg);
    if (!dto) {
        callback(common::ApiResponse::error(drogon::k400BadRequest, errorMsg));
        return;
    }

    auto span = req ? req->getAttributes()->get<otel::SpanPtr>("otel.span") : nullptr;

    services::UserService::instance().createUser(
        *dto,
        [callback = std::move(callback)](bool ok, drogon::HttpStatusCode status, const std::optional<models::User>& user, const std::string& error) {
            if (!ok) {
                callback(common::ApiResponse::error(status, error));
                return;
            }
            if (!user) {
                callback(common::ApiResponse::error(drogon::k500InternalServerError, "Failed to create user"));
                return;
            }

            auto resp = common::ApiResponse::successWithPayload([&user](rapidjson::Writer<rapidjson::StringBuffer>& writer) {
                user->serialize(writer);
            }, "User created successfully", drogon::k201Created);

            callback(resp);
        },
        span);
}

void UserController::updateUser(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    int64_t id) 
{
    std::string errorMsg;
    auto dto = models::UpdateUserDto::fromJson(req->getBody(), &errorMsg);
    if (!dto) {
        callback(common::ApiResponse::error(drogon::k400BadRequest, errorMsg));
        return;
    }

    auto span = req ? req->getAttributes()->get<otel::SpanPtr>("otel.span") : nullptr;

    services::UserService::instance().updateUser(
        id, *dto,
        [id, callback = std::move(callback)](bool ok, const std::optional<models::User>& user, const std::string& error) {
            if (!ok) {
                callback(common::ApiResponse::error(drogon::k400BadRequest, error));
                return;
            }
            if (!user) {
                callback(common::ApiResponse::error(drogon::k404NotFound, "User not found with id " + std::to_string(id)));
                return;
            }

            auto resp = common::ApiResponse::successWithPayload([&user](rapidjson::Writer<rapidjson::StringBuffer>& writer) {
                user->serialize(writer);
            }, "User updated successfully");

            callback(resp);
        },
        span);
}

void UserController::deleteUser(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback,
    int64_t id) 
{
    auto span = req ? req->getAttributes()->get<otel::SpanPtr>("otel.span") : nullptr;

    services::UserService::instance().deleteUser(
        id,
        [id, callback = std::move(callback)](bool ok, bool found, const std::string& error) {
            if (!ok) {
                callback(common::ApiResponse::error(drogon::k500InternalServerError, error));
                return;
            }
            if (!found) {
                callback(common::ApiResponse::error(drogon::k404NotFound, "User not found with id " + std::to_string(id)));
                return;
            }

            callback(common::ApiResponse::success("User deleted successfully"));
        },
        span);
}

} // namespace api::controllers
