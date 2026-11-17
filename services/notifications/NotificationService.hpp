#pragma once
#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>
#include <json/json.h>
#include <string>
#include "input/QueryInputs.hpp"
#include "models/Notifications.h"
#include "services/realtime/RealtimeService.hpp"

using drogon_model::evento::Notifications;

struct NotificationPage {
    size_t count = 0;
    Json::Value items = Json::Value(Json::arrayValue);   // [{"id", "message", "created_at"}], newest first
};

class NotificationService {
    public:
        NotificationService(drogon::orm::DbClientPtr dbClient, RealtimeService* realtime)
            : dbClient_(std::move(dbClient)), realtime_(realtime) {}

        drogon::Task<Notifications> create(const std::string& userId, const std::string& message);

        // The user's saved notifications, newest first. created_at is ISO 8601 UTC.
        drogon::Task<NotificationPage> list(const std::string& userId, const PageInput& page);

        drogon::Task<> push(const std::string& userId, const std::string& message);

    private:
        drogon::orm::DbClientPtr dbClient_;
        RealtimeService* realtime_;
};
