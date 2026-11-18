#include "NotificationService.hpp"
#include <drogon/orm/CoroMapper.h>
#include <json/json.h>

using namespace drogon::orm;
using drogon::Task;

Task<Notifications> NotificationService::create(const std::string& userId, const std::string& message) {
    CoroMapper<Notifications> notifications(dbClient_);
    Notifications notification;
    notification.setUserId(userId);
    notification.setMessage(message);
    co_return co_await notifications.insert(notification);
}

Task<> NotificationService::push(const std::string& userId, const std::string& message) {
    Json::Value body;
    body["detail"]  = "Notification Message";
    body["message"] = message;
    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    co_await realtime_->publish(RealtimeService::notificationsChannel(userId), Json::writeString(writer, body));
}

Task<NotificationPage> NotificationService::list(const std::string& userId, const PageInput& page) {
    NotificationPage result;
    result.count = co_await CoroMapper<Notifications>(dbClient_).count(Criteria(Notifications::Cols::_user_id, userId));

    const auto rows = co_await dbClient_->execSqlCoro(
        "SELECT id, message, "
        "to_char(created_at AT TIME ZONE 'UTC', 'YYYY-MM-DD\"T\"HH24:MI:SS\"Z\"') AS created_at "
        "FROM notifications WHERE user_id = $1 "
        "ORDER BY notifications.created_at DESC, notifications.id DESC "
        "LIMIT $2 OFFSET $3",
        userId, static_cast<int64_t>(page.limit), static_cast<int64_t>(page.offset));

    for (const auto& row : rows) {
        Json::Value item;
        item["id"]         = static_cast<Json::Int64>(row["id"].as<int64_t>());
        item["message"]    = row["message"].as<std::string>();
        item["created_at"] = row["created_at"].as<std::string>();
        result.items.append(item);
    }
    co_return result;
}
