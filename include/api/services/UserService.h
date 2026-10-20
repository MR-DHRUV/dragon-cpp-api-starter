#pragma once

#include "api/models/User.h"
#include "api/otel/ScopedSpan.h"
#include <functional>
#include <vector>
#include <optional>
#include <string>
#include <memory>
#include <drogon/HttpTypes.h>

namespace api::services {

class UserService {
public:
    static UserService& instance() {
        static UserService inst;
        return inst;
    }

    using UsersCallback = std::function<void(bool ok, const std::vector<models::User>& users, const std::string& error)>;
    using SingleUserCallback = std::function<void(bool ok, const std::optional<models::User>& user, const std::string& error)>;
    using DeleteCallback = std::function<void(bool ok, bool found, const std::string& error)>;
    using AvailabilityCallback = std::function<void(bool queryOk, bool available, bool validFormat, const std::string& reason)>;
    using CreateUserCallback = std::function<void(bool ok, drogon::HttpStatusCode status, const std::optional<models::User>& user, const std::string& error)>;

    void checkUsernameAvailability(const std::string& username, AvailabilityCallback cb, const otel::SpanPtr& parentSpan = nullptr);
    void getAllUsers(size_t limit, size_t offset, UsersCallback cb, const otel::SpanPtr& parentSpan = nullptr);
    void getUserById(int64_t id, SingleUserCallback cb, const otel::SpanPtr& parentSpan = nullptr);
    void createUser(const models::CreateUserDto& dto, CreateUserCallback cb, const otel::SpanPtr& parentSpan = nullptr);
    void updateUser(int64_t id, const models::UpdateUserDto& dto, SingleUserCallback cb, const otel::SpanPtr& parentSpan = nullptr);
    void deleteUser(int64_t id, DeleteCallback cb, const otel::SpanPtr& parentSpan = nullptr);

private:
    UserService() = default;
};

} // namespace api::services
