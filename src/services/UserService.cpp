#include "api/services/UserService.h"
#include "api/db/DatabaseManager.h"
#include <trantor/utils/Logger.h>

namespace api::services {

void UserService::getAllUsers(size_t limit, size_t offset, UsersCallback cb) {
    auto client = db::DatabaseManager::instance().getClient();
    if (!client) {
        cb(false, {}, "Database client not available");
        return;
    }

    *client << "SELECT id, username, email, full_name, is_active, created_at, updated_at "
               "FROM users ORDER BY id ASC LIMIT $1 OFFSET $2;"
            << static_cast<int64_t>(limit)
            << static_cast<int64_t>(offset)
        >> [cb = std::move(cb)](const drogon::orm::Result& result) {
            std::vector<models::User> users;
            users.reserve(result.size());
            for (const auto& row : result) {
                users.push_back(models::User::fromDbRow(row));
            }
            cb(true, users, "");
        }
        >> [cb = std::move(cb)](const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "Failed to query users: " << e.base().what();
            cb(false, {}, e.base().what());
        };
}

void UserService::getUserById(int64_t id, SingleUserCallback cb) {
    auto client = db::DatabaseManager::instance().getClient();
    if (!client) {
        cb(false, std::nullopt, "Database client not available");
        return;
    }

    *client << "SELECT id, username, email, full_name, is_active, created_at, updated_at "
               "FROM users WHERE id = $1;"
            << id
        >> [cb = std::move(cb)](const drogon::orm::Result& result) {
            if (result.empty()) {
                cb(true, std::nullopt, "");
            } else {
                cb(true, models::User::fromDbRow(result[0]), "");
            }
        }
        >> [cb = std::move(cb)](const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "Failed to query user by id: " << e.base().what();
            cb(false, std::nullopt, e.base().what());
        };
}

void UserService::checkUsernameAvailability(const std::string& username, AvailabilityCallback cb) {
    std::string formatError;
    if (!models::isValidUsername(username, &formatError)) {
        cb(true, false, false, formatError);
        return;
    }

    auto client = db::DatabaseManager::instance().getClient();
    if (!client) {
        cb(false, false, true, "Database client not available");
        return;
    }

    *client << "SELECT id FROM users WHERE LOWER(username) = LOWER($1) LIMIT 1;"
            << username
        >> [cb = std::move(cb), username](const drogon::orm::Result& result) {
            if (!result.empty()) {
                cb(true, false, true, "Username '" + username + "' is already taken");
            } else {
                cb(true, true, true, "Username is available");
            }
        }
        >> [cb = std::move(cb)](const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "Failed to check username availability: " << e.base().what();
            cb(false, false, true, e.base().what());
        };
}

void UserService::createUser(const models::CreateUserDto& dto, CreateUserCallback cb) {
    // 1. Re-verify format
    std::string formatError;
    if (!models::isValidUsername(dto.username, &formatError)) {
        cb(false, drogon::k400BadRequest, std::nullopt, formatError);
        return;
    }

    auto client = db::DatabaseManager::instance().getClient();
    if (!client) {
        cb(false, drogon::k500InternalServerError, std::nullopt, "Database client not available");
        return;
    }

    // 2. Re-verify availability against database before insertion
    *client << "SELECT id FROM users WHERE LOWER(username) = LOWER($1) LIMIT 1;"
            << dto.username
        >> [this, client, dto, cb = std::move(cb)](const drogon::orm::Result& checkResult) {
            if (!checkResult.empty()) {
                cb(false, drogon::k409Conflict, std::nullopt,
                   "Username '" + dto.username + "' is already taken. Please choose another username.");
                return;
            }

            // 3. Username verified available, proceed to insert
            *client << "INSERT INTO users (username, email, full_name) VALUES ($1, $2, $3) "
                       "RETURNING id, username, email, full_name, is_active, created_at, updated_at;"
                    << dto.username
                    << dto.email
                    << dto.fullName
                >> [cb](const drogon::orm::Result& result) {
                    if (result.empty()) {
                        cb(false, drogon::k500InternalServerError, std::nullopt, "Insert succeeded but no row returned");
                    } else {
                        cb(true, drogon::k201Created, models::User::fromDbRow(result[0]), "");
                    }
                }
                >> [cb, dto](const drogon::orm::DrogonDbException& e) {
                    std::string err = e.base().what();
                    LOG_ERROR << "Failed to insert user: " << err;
                    if (err.find("users_username_key") != std::string::npos ||
                        (err.find("duplicate key") != std::string::npos && err.find("username") != std::string::npos)) {
                        cb(false, drogon::k409Conflict, std::nullopt,
                           "Username '" + dto.username + "' is already taken. Please choose another username.");
                    } else if (err.find("users_email_key") != std::string::npos ||
                               (err.find("duplicate key") != std::string::npos && err.find("email") != std::string::npos)) {
                        cb(false, drogon::k409Conflict, std::nullopt,
                           "Email '" + dto.email + "' is already registered.");
                    } else {
                        cb(false, drogon::k500InternalServerError, std::nullopt, err);
                    }
                };
        }
        >> [cb](const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "Username availability reverification failed: " << e.base().what();
            cb(false, drogon::k500InternalServerError, std::nullopt, e.base().what());
        };
}

void UserService::updateUser(int64_t id, const models::UpdateUserDto& dto, SingleUserCallback cb) {
    auto client = db::DatabaseManager::instance().getClient();
    if (!client) {
        cb(false, std::nullopt, "Database client not available");
        return;
    }

    // First fetch existing user to update non-null fields
    getUserById(id, [this, client, id, dto, cb = std::move(cb)](bool ok, const std::optional<models::User>& existing, const std::string& err) {
        if (!ok) {
            cb(false, std::nullopt, err);
            return;
        }
        if (!existing) {
            cb(true, std::nullopt, "");
            return;
        }

        std::string newEmail = dto.email ? *dto.email : existing->email;
        std::string newFullName = dto.fullName ? *dto.fullName : existing->fullName;
        bool newIsActive = dto.isActive ? *dto.isActive : existing->isActive;

        *client << "UPDATE users SET email = $1, full_name = $2, is_active = $3, updated_at = CURRENT_TIMESTAMP "
                   "WHERE id = $4 RETURNING id, username, email, full_name, is_active, created_at, updated_at;"
                << newEmail
                << newFullName
                << newIsActive
                << id
            >> [cb = std::move(cb)](const drogon::orm::Result& result) {
                if (result.empty()) {
                    cb(true, std::nullopt, "");
                } else {
                    cb(true, models::User::fromDbRow(result[0]), "");
                }
            }
            >> [cb = std::move(cb)](const drogon::orm::DrogonDbException& e) {
                LOG_ERROR << "Failed to update user: " << e.base().what();
                cb(false, std::nullopt, e.base().what());
            };
    });
}

void UserService::deleteUser(int64_t id, DeleteCallback cb) {
    auto client = db::DatabaseManager::instance().getClient();
    if (!client) {
        cb(false, false, "Database client not available");
        return;
    }

    *client << "DELETE FROM users WHERE id = $1;"
            << id
        >> [cb = std::move(cb)](const drogon::orm::Result& result) {
            bool found = (result.affectedRows() > 0);
            cb(true, found, "");
        }
        >> [cb = std::move(cb)](const drogon::orm::DrogonDbException& e) {
            LOG_ERROR << "Failed to delete user: " << e.base().what();
            cb(false, false, e.base().what());
        };
}

} // namespace api::services
