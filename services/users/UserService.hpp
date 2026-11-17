#pragma once
#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>
#include <json/json.h>
#include <optional>
#include <string>
#include <vector>
#include "models/Users.h"
#include "input/UserInputs.hpp"
#include "events/EventBus.hpp"

using drogon_model::evento::Users;

struct SocialProfile {
    std::string email;
    std::string fullName;
    std::string picture;
    std::string provider;
};

class UserService {
    public:
        UserService(drogon::orm::DbClientPtr dbClient, EventBus* events, std::string frontendLink)
            : dbClient_(std::move(dbClient)), events_(events), frontendLink_(std::move(frontendLink)) {}

        drogon::Task<Users> getUserById(const std::string& id);
        drogon::Task<std::optional<Users>> findByEmail(const std::string& email);

        drogon::Task<Users> createUser(const RegisterInput& input);
        drogon::Task<std::pair<Users, bool>> findOrCreateSocialUser(const SocialProfile& profile);
        drogon::Task<Users> updateUser(const std::string& id, const UpdateUserInput& input);
        drogon::Task<std::vector<std::string>> deleteUser(const std::string& id);

        drogon::Task<bool> checkPassword(const Users& user, const std::string& password);
        drogon::Task<> setLastLogin(const std::string& id);
        drogon::Task<> setLocation(const std::string& id, const std::string& location);

        drogon::Task<Json::Value> userJson(const std::string& id);
        drogon::Task<Json::Value> usersJson(const std::vector<std::string>& ids);
        drogon::Task<Json::Value> allUsersJson();

    private:

        drogon::orm::DbClientPtr dbClient_;
        EventBus* events_;
        std::string frontendLink_;
};
