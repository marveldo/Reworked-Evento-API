#pragma once
#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>
#include <json/json.h>
#include <optional>
#include <string>
#include <vector>
#include "models/DeviceInfo.h"
#include "models/Users.h"
#include "events/EventBus.hpp"
#include "services/jwt/JwtService.hpp"

using drogon_model::evento::DeviceInfo;

struct DeviceDetailsInput {
    std::string ipAddress;
    std::string deviceName;
    std::string deviceOs;
    std::string deviceType;
};

struct DeviceSession {
    std::string userId;
    std::string deviceId;
    std::string accessToken;
    std::string refreshToken;
    std::string accessexpirytime;
    std::string refreshexpirytime;
};

class DeviceService {
    public:
        DeviceService(drogon::orm::DbClientPtr dbClient, EventBus* events, JwtService* jwt)
            : dbClient_(std::move(dbClient)), events_(events), jwt_(jwt) {}

        drogon::Task<DeviceSession> startSession(const drogon_model::evento::Users& user, const DeviceDetailsInput& details);

        drogon::Task<std::optional<DeviceSession>> refreshSession(const std::string& refreshToken);

        drogon::Task<bool> isActive(const std::string& deviceId, const std::string& userId);
        drogon::Task<bool> isCurrentAccessToken(const std::string& deviceId, const std::string& userId, const std::string& token);

        drogon::Task<Json::Value> listForUser(const std::string& userId);
        drogon::Task<bool> endSession(const std::string& deviceId, const std::string& userId);

    private:
        drogon::orm::DbClientPtr dbClient_;
        EventBus* events_;
        JwtService* jwt_;
};
