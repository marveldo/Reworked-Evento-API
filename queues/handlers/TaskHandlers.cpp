#include "TaskHandlers.hpp"
#include "TaskMiddlewares.hpp"
#include "utils/DbErrors.hpp"
#include "queues/TaskErrors.hpp"
#include "settings/Settings.h"
#include <drogon/orm/Exception.h>

void registerTaskHandlers(TaskQueue* queue, EmailService* email, Services* services) {
    queue->registerHandler("send_email", [email](const nlohmann::json& payload) -> drogon::Task<> {
        co_await email->send(payload.at("to").get<std::string>(),
                             payload.at("subject").get<std::string>(),
                             payload.at("template").get<std::string>(),
                             payload.value("data", nlohmann::json::object()));
    }, {timed("send_email")});

    queue->registerHandler("create_notification", [services](const nlohmann::json& payload) -> drogon::Task<> {
        const std::string userId  = payload.at("user_id").get<std::string>();
        const std::string message = payload.at("message").get<std::string>();
        try {
            co_await services->notificationService->create(userId, message);
        } catch (const drogon::orm::Failure& e) {
            if (isForeignKeyViolation(e)) throw PermanentTaskError("the user was deleted before the notification was saved");
            throw;
        }
        try {
            co_await services->notificationService->push(userId, message);
        } catch (const std::exception& e) {
            LOG_WARN << "create_notification: saved, but the push to " << userId << " failed: " << e.what();
        }
    }, {timed("create_notification")});

    queue->registerHandler("send_logout", [services](const nlohmann::json& payload) -> drogon::Task<> {
        const std::string deviceId = payload.at("device_id").get<std::string>();
        co_await services->realtimeService->publish(RealtimeService::logoutChannel(deviceId), "Device Logged Out");
    }, {timed("send_logout")});

    queue->registerHandler("update_user_location", [services](const nlohmann::json& payload) -> drogon::Task<> {
        if (!getSettings()->geo.enabled) co_return;
        const std::string userId = payload.at("user_id").get<std::string>();
        const auto location = co_await services->geoService->locate(payload.value("ip_address", ""));
        if (location) co_await services->userService->setLocation(userId, *location);
    }, {timed("update_user_location")});

    queue->registerHandler("delete_photo", [services](const nlohmann::json& payload) -> drogon::Task<> {
        co_await services->cloudinaryService->deleteByPrefix(payload.at("public_id").get<std::string>());
    }, {timed("delete_photo")});
}
