#include "api/common/RapidJsonHelper.h"
#include "api/models/User.h"
#include <iostream>
#include <cassert>

void testJsonHelperParsing() {
    std::string json = R"({"name":"John Doe","age":30,"active":true,"score":98.5})";
    std::string err;
    auto doc = api::common::RapidJsonHelper::parse(json, &err);
    assert(doc.has_value());
    assert(doc->IsObject());

    auto name = api::common::RapidJsonHelper::getString(*doc, "name");
    assert(name && *name == "John Doe");

    auto age = api::common::RapidJsonHelper::getInt(*doc, "age");
    assert(age && *age == 30);

    auto active = api::common::RapidJsonHelper::getBool(*doc, "active");
    assert(active && *active == true);

    auto score = api::common::RapidJsonHelper::getDouble(*doc, "score");
    assert(score && *score > 98.4 && *score < 98.6);

    std::cout << "[PASS] RapidJsonHelper parsing tests\n";
}

void testUserSerialization() {
    api::models::User user;
    user.id = 42;
    user.username = "turbo_dev";
    user.email = "turbo@speed.io";
    user.fullName = "Turbo Developer";
    user.isActive = true;
    user.createdAt = "2026-10-18T22:00:00Z";
    user.updatedAt = "2026-10-18T22:00:00Z";

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    user.serialize(writer);

    std::string output = buffer.GetString();
    assert(output.find("turbo_dev") != std::string::npos);
    assert(output.find("42") != std::string::npos);

    std::cout << "[PASS] User streaming serialization tests\n";
}

void testUsernameValidation() {
    std::string reason;
    assert(api::models::isValidUsername("john_doe", &reason));
    assert(api::models::isValidUsername("user123", &reason));
    assert(api::models::isValidUsername("abc", &reason)); // minimum 3 chars

    assert(!api::models::isValidUsername("", &reason)); // empty
    assert(!api::models::isValidUsername("ab", &reason)); // too short (<3)
    assert(!api::models::isValidUsername("this_username_is_way_too_long_to_be_allowed_1234567890", &reason)); // >30 chars
    assert(!api::models::isValidUsername("john-doe", &reason)); // hyphens not allowed (only alphanumeric + underscore)
    assert(!api::models::isValidUsername("user@name", &reason)); // invalid char @
    assert(!api::models::isValidUsername("user name", &reason)); // space not allowed

    std::cout << "[PASS] Username validation tests\n";
}

int main() {
    std::cout << "Running Unit Tests...\n";
    testJsonHelperParsing();
    testUserSerialization();
    testUsernameValidation();
    std::cout << "All unit tests completed successfully!\n";
    return 0;
}