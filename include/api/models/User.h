#pragma once

#include "../common/RapidJsonHelper.h"
#include <drogon/orm/Field.h>
#include <drogon/orm/Row.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

#include <string>
#include <optional>
#include <cstdint>

namespace api::models {

struct User {
    int64_t id{0};
    std::string username;
    std::string email;
    std::string fullName;
    bool isActive{true};
    std::string createdAt;
    std::string updatedAt;

    /**
     * @brief Streaming serializer using RapidJSON Writer.
     */
    void serialize(rapidjson::Writer<rapidjson::StringBuffer>& writer) const {
        writer.StartObject();
        
        writer.Key("id");
        writer.Int64(id);

        writer.Key("username");
        writer.String(username.c_str(), static_cast<rapidjson::SizeType>(username.length()));

        writer.Key("email");
        writer.String(email.c_str(), static_cast<rapidjson::SizeType>(email.length()));

        writer.Key("fullName");
        writer.String(fullName.c_str(), static_cast<rapidjson::SizeType>(fullName.length()));

        writer.Key("isActive");
        writer.Bool(isActive);

        writer.Key("createdAt");
        writer.String(createdAt.c_str(), static_cast<rapidjson::SizeType>(createdAt.length()));

        writer.Key("updatedAt");
        writer.String(updatedAt.c_str(), static_cast<rapidjson::SizeType>(updatedAt.length()));

        writer.EndObject();
    }

    /**
     * @brief Constructs a User instance from a Drogon ORM Row.
     */
    static User fromDbRow(const drogon::orm::Row& row) {
        User user;
        user.id = row["id"].as<int64_t>();
        user.username = row["username"].as<std::string>();
        user.email = row["email"].as<std::string>();
        user.fullName = row["full_name"].as<std::string>();
        user.isActive = row["is_active"].as<bool>();
        user.createdAt = row["created_at"].as<std::string>();
        user.updatedAt = row["updated_at"].as<std::string>();
        return user;
    }
};

/**
 * @brief Validates username constraints (3-30 chars, alphanumeric + underscores).
 */
inline bool isValidUsername(std::string_view username, std::string* reason = nullptr) {
    if (username.empty()) {
        if (reason) *reason = "Username cannot be empty";
        return false;
    }
    if (username.length() < 3 || username.length() > 30) {
        if (reason) *reason = "Username must be between 3 and 30 characters in length";
        return false;
    }
    for (char c : username) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
            if (reason) *reason = "Username can only contain alphanumeric characters and underscores";
            return false;
        }
    }
    return true;
}

struct CreateUserDto {
    std::string username;
    std::string email;
    std::string fullName;

    static std::optional<CreateUserDto> fromJson(std::string_view jsonStr, std::string* errorMsg = nullptr) {
        auto docOpt = common::RapidJsonHelper::parse(jsonStr, errorMsg);
        if (!docOpt || !docOpt->IsObject()) {
            if (errorMsg && errorMsg->empty()) *errorMsg = "Root JSON must be an object";
            return std::nullopt;
        }

        const auto& doc = *docOpt;
        auto u = common::RapidJsonHelper::getString(doc, "username");
        auto e = common::RapidJsonHelper::getString(doc, "email");
        auto f = common::RapidJsonHelper::getString(doc, "fullName");

        if (!u || u->empty()) {
            if (errorMsg) *errorMsg = "Field 'username' is required and cannot be empty";
            return std::nullopt;
        }

        if (!isValidUsername(*u, errorMsg)) {
            return std::nullopt;
        }

        if (!e || e->empty()) {
            if (errorMsg) *errorMsg = "Field 'email' is required and cannot be empty";
            return std::nullopt;
        }

        CreateUserDto dto;
        dto.username = std::move(*u);
        dto.email = std::move(*e);
        dto.fullName = f.value_or("");
        return dto;
    }
};

struct UpdateUserDto {
    std::optional<std::string> email;
    std::optional<std::string> fullName;
    std::optional<bool> isActive;

    static std::optional<UpdateUserDto> fromJson(std::string_view jsonStr, std::string* errorMsg = nullptr) {
        auto docOpt = common::RapidJsonHelper::parse(jsonStr, errorMsg);
        if (!docOpt || !docOpt->IsObject()) {
            if (errorMsg && errorMsg->empty()) *errorMsg = "Root JSON must be an object";
            return std::nullopt;
        }

        const auto& doc = *docOpt;
        UpdateUserDto dto;
        dto.email = common::RapidJsonHelper::getString(doc, "email");
        dto.fullName = common::RapidJsonHelper::getString(doc, "fullName");
        dto.isActive = common::RapidJsonHelper::getBool(doc, "isActive");
        return dto;
    }
};

} // namespace api::models
