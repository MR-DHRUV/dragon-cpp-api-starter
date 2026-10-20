#include "api/services/UserService.h"
#include "api/db/DatabaseManager.h"
#include <trantor/utils/Logger.h>
#include <chrono>
#include <atomic>

namespace api::services {

void UserService::getAllUsers(size_t limit, size_t offset, UsersCallback cb, const otel::SpanPtr& parentSpan) {
    auto span = std::make_shared<otel::ScopedSpan>("UserService::getAllUsers", parentSpan, otel::SpanKind::Internal);
    span->attr("query.limit", static_cast<int64_t>(limit));
    span->attr("query.offset", static_cast<int64_t>(offset));

    auto invoked = std::make_shared<std::atomic<bool>>(false);
    auto cbPtr = std::make_shared<UsersCallback>(std::move(cb));
    auto done = [span, cbPtr, invoked](bool ok, const std::vector<models::User>& users, const std::string& error) {
        if (invoked->exchange(true)) return;
        if (!ok) {
            span->fail(error);
        }
        span->end();
        if (*cbPtr) (*cbPtr)(ok, users, error);
    };

    auto client = db::DatabaseManager::instance().getClient();
    if (!client) {
        done(false, {}, "Database client not available");
        return;
    }

    db::tracedExec(
        client, span, "SELECT users",
        "SELECT id, username, email, full_name, is_active, created_at, updated_at FROM users ORDER BY id ASC LIMIT $1 OFFSET $2;",
        [done](const drogon::orm::Result& result) {
            std::vector<models::User> users;
            users.reserve(result.size());
            for (const auto& row : result) {
                users.push_back(models::User::fromDbRow(row));
            }
            done(true, users, "");
        },
        [done](const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "Failed to query users: " << e.base().what();
            done(false, {}, e.base().what());
        },
        static_cast<int64_t>(limit),
        static_cast<int64_t>(offset)
    );
}

void UserService::getUserById(int64_t id, SingleUserCallback cb, const otel::SpanPtr& parentSpan) {
    auto span = std::make_shared<otel::ScopedSpan>("UserService::getUserById", parentSpan, otel::SpanKind::Internal);
    span->attr("user.id", id);

    auto invoked = std::make_shared<std::atomic<bool>>(false);
    auto cbPtr = std::make_shared<SingleUserCallback>(std::move(cb));
    auto done = [span, cbPtr, invoked](bool ok, const std::optional<models::User>& user, const std::string& error) {
        if (invoked->exchange(true)) return;
        if (!ok) {
            span->fail(error);
        }
        span->end();
        if (*cbPtr) (*cbPtr)(ok, user, error);
    };

    auto client = db::DatabaseManager::instance().getClient();
    if (!client) {
        done(false, std::nullopt, "Database client not available");
        return;
    }

    db::tracedExec(
        client, span, "SELECT users WHERE id",
        "SELECT id, username, email, full_name, is_active, created_at, updated_at FROM users WHERE id = $1;",
        [done](const drogon::orm::Result& result) {
            if (result.empty()) {
                done(true, std::nullopt, "");
            } else {
                done(true, models::User::fromDbRow(result[0]), "");
            }
        },
        [done](const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "Failed to query user by id: " << e.base().what();
            done(false, std::nullopt, e.base().what());
        },
        id
    );
}

void UserService::checkUsernameAvailability(const std::string& username, AvailabilityCallback cb, const otel::SpanPtr& parentSpan) {
    auto span = std::make_shared<otel::ScopedSpan>("UserService::checkUsernameAvailability", parentSpan, otel::SpanKind::Internal);
    span->attr("user.username", username);

    auto invoked = std::make_shared<std::atomic<bool>>(false);
    auto cbPtr = std::make_shared<AvailabilityCallback>(std::move(cb));
    auto done = [span, cbPtr, invoked](bool queryOk, bool available, bool validFormat, const std::string& reason) {
        if (invoked->exchange(true)) return;
        if (!queryOk || !validFormat) {
            span->fail(reason);
        }
        span->end();
        if (*cbPtr) (*cbPtr)(queryOk, available, validFormat, reason);
    };

    std::string formatError;
    if (!models::isValidUsername(username, &formatError)) {
        done(true, false, false, formatError);
        return;
    }

    auto client = db::DatabaseManager::instance().getClient();
    if (!client) {
        done(false, false, true, "Database client not available");
        return;
    }

    db::tracedExec(
        client, span, "SELECT users",
        "SELECT id FROM users WHERE LOWER(username) = LOWER($1) LIMIT 1;",
        [done, username](const drogon::orm::Result& result) {
            if (!result.empty()) {
                done(true, false, true, "Username '" + username + "' is already taken");
            } else {
                done(true, true, true, "Username is available");
            }
        },
        [done](const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "Failed to check username availability: " << e.base().what();
            done(false, false, true, e.base().what());
        },
        username
    );
}

void UserService::createUser(const models::CreateUserDto& dto, CreateUserCallback cb, const otel::SpanPtr& parentSpan) {
    auto span = std::make_shared<otel::ScopedSpan>("UserService::createUser", parentSpan, otel::SpanKind::Internal);
    span->attr("user.username", dto.username);
    span->attr("user.email", dto.email);

    auto invoked = std::make_shared<std::atomic<bool>>(false);
    auto cbPtr = std::make_shared<CreateUserCallback>(std::move(cb));
    auto done = [span, cbPtr, invoked](bool ok, drogon::HttpStatusCode status, const std::optional<models::User>& user, const std::string& error) {
        if (invoked->exchange(true)) return;
        if (!ok) {
            span->fail(error);
        }
        span->end();
        if (*cbPtr) (*cbPtr)(ok, status, user, error);
    };

    // 1. Re-verify format
    std::string formatError;
    if (!models::isValidUsername(dto.username, &formatError)) {
        done(false, drogon::k400BadRequest, std::nullopt, formatError);
        return;
    }

    auto client = db::DatabaseManager::instance().getClient();
    if (!client) {
        done(false, drogon::k500InternalServerError, std::nullopt, "Database client not available");
        return;
    }

    // 2. Check duplicate username
    db::tracedExec(
        client, span, "SELECT users (check duplicate)",
        "SELECT id FROM users WHERE LOWER(username) = LOWER($1) LIMIT 1;",
        [this, client, dto, done, span](const drogon::orm::Result& checkResult) {
            if (!checkResult.empty()) {
                done(false, drogon::k409Conflict, std::nullopt,
                     "Username '" + dto.username + "' is already taken. Please choose another username.");
                return;
            }

            // 3. Insert user
            db::tracedExec(
                client, span, "INSERT users",
                "INSERT INTO users (username, email, full_name) VALUES ($1, $2, $3) RETURNING id, username, email, full_name, is_active, created_at, updated_at;",
                [done](const drogon::orm::Result& result) {
                    if (result.empty()) {
                        done(false, drogon::k500InternalServerError, std::nullopt, "Insert succeeded but no row returned");
                    } else {
                        done(true, drogon::k201Created, models::User::fromDbRow(result[0]), "");
                    }
                },
                [done, dto](const drogon::orm::DrogonDbException& e) {
                    std::string err = e.base().what();
                    LOG_ERROR << "Failed to insert user: " << err;
                    if (err.find("users_username_key") != std::string::npos ||
                        (err.find("duplicate key") != std::string::npos && err.find("username") != std::string::npos)) {
                        done(false, drogon::k409Conflict, std::nullopt,
                             "Username '" + dto.username + "' is already taken. Please choose another username.");
                    } else if (err.find("users_email_key") != std::string::npos ||
                               (err.find("duplicate key") != std::string::npos && err.find("email") != std::string::npos)) {
                        done(false, drogon::k409Conflict, std::nullopt,
                             "Email '" + dto.email + "' is already registered.");
                    } else {
                        done(false, drogon::k500InternalServerError, std::nullopt, err);
                    }
                },
                dto.username,
                dto.email,
                dto.fullName
            );
        },
        [done](const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "Username availability reverification failed: " << e.base().what();
            done(false, drogon::k500InternalServerError, std::nullopt, e.base().what());
        },
        dto.username
    );
}

void UserService::updateUser(int64_t id, const models::UpdateUserDto& dto, SingleUserCallback cb, const otel::SpanPtr& parentSpan) {
    auto span = std::make_shared<otel::ScopedSpan>("UserService::updateUser", parentSpan, otel::SpanKind::Internal);
    span->attr("user.id", id);

    auto invoked = std::make_shared<std::atomic<bool>>(false);
    auto cbPtr = std::make_shared<SingleUserCallback>(std::move(cb));
    auto done = [span, cbPtr, invoked](bool ok, const std::optional<models::User>& user, const std::string& error) {
        if (invoked->exchange(true)) return;
        if (!ok) {
            span->fail(error);
        }
        span->end();
        if (*cbPtr) (*cbPtr)(ok, user, error);
    };

    auto client = db::DatabaseManager::instance().getClient();
    if (!client) {
        done(false, std::nullopt, "Database client not available");
        return;
    }

    // First fetch existing user to update non-null fields
    getUserById(id, [this, client, id, dto, done, span](bool ok, const std::optional<models::User>& existing, const std::string& err) {
        if (!ok) {
            done(false, std::nullopt, err);
            return;
        }
        if (!existing) {
            done(true, std::nullopt, "");
            return;
        }

        std::string newEmail = dto.email ? *dto.email : existing->email;
        std::string newFullName = dto.fullName ? *dto.fullName : existing->fullName;
        bool newIsActive = dto.isActive ? *dto.isActive : existing->isActive;

        db::tracedExec(
            client, span, "UPDATE users",
            "UPDATE users SET email = $1, full_name = $2, is_active = $3, updated_at = CURRENT_TIMESTAMP WHERE id = $4 RETURNING id, username, email, full_name, is_active, created_at, updated_at;",
            [done](const drogon::orm::Result& result) {
                if (result.empty()) {
                    done(true, std::nullopt, "");
                } else {
                    done(true, models::User::fromDbRow(result[0]), "");
                }
            },
            [done](const drogon::orm::DrogonDbException& e) {
                LOG_ERROR << "Failed to update user: " << e.base().what();
                done(false, std::nullopt, e.base().what());
            },
            newEmail,
            newFullName,
            newIsActive,
            id
        );
    }, span);
}

void UserService::deleteUser(int64_t id, DeleteCallback cb, const otel::SpanPtr& parentSpan) {
    auto span = std::make_shared<otel::ScopedSpan>("UserService::deleteUser", parentSpan, otel::SpanKind::Internal);
    span->attr("user.id", id);

    auto invoked = std::make_shared<std::atomic<bool>>(false);
    auto cbPtr = std::make_shared<DeleteCallback>(std::move(cb));
    auto done = [span, cbPtr, invoked](bool ok, bool found, const std::string& error) {
        if (invoked->exchange(true)) return;
        if (!ok) {
            span->fail(error);
        }
        span->end();
        if (*cbPtr) (*cbPtr)(ok, found, error);
    };

    auto client = db::DatabaseManager::instance().getClient();
    if (!client) {
        done(false, false, "Database client not available");
        return;
    }

    db::tracedExec(
        client, span, "DELETE users",
        "DELETE FROM users WHERE id = $1;",
        [done](const drogon::orm::Result& result) {
            bool found = (result.affectedRows() > 0);
            done(true, found, "");
        },
        [done](const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "Failed to delete user: " << e.base().what();
            done(false, false, e.base().what());
        },
        id
    );
}

} // namespace api::services
