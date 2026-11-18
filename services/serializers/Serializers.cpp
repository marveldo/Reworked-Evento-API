#include "Serializers.hpp"
#include <drogon/orm/CoroMapper.h>
#include <unordered_set>

using drogon::Task;
using drogon::orm::DbClientPtr;
using drogon::orm::Result;
using drogon::orm::Row;
using drogon_model::evento::Events;
using drogon_model::evento::Users;

namespace {
    Json::Value text(const drogon::orm::Field& field) {
        return field.isNull() ? Json::Value() : Json::Value(field.as<std::string>());
    }

    Json::Value integer(const drogon::orm::Field& field) {
        return field.isNull() ? Json::Value() : Json::Value(static_cast<Json::Int64>(field.as<int64_t>()));
    }

    Json::Value text(const std::shared_ptr<std::string>& value) {
        return value ? Json::Value(*value) : Json::Value();
    }

    Json::Value integer(const std::shared_ptr<int32_t>& value) {
        return value ? Json::Value(static_cast<Json::Int64>(*value)) : Json::Value();
    }

    Json::Value day(const std::shared_ptr<trantor::Date>& value) {
        return value ? Json::Value(value->toCustomFormattedStringLocal("%Y-%m-%d")) : Json::Value();
    }
}

namespace serializers {

std::string uuidArray(const std::vector<std::string>& ids) {
    std::string out = "{";
    for (size_t i = 0; i < ids.size(); i++) out += (i ? "," : "") + ids[i];
    return out + "}";
}

Json::Value event(const Row& row, const std::string& frontendLink) {
    Json::Value e;
    e["id"]          = row["id"].as<std::string>();
    e["event_name"]  = text(row["event_name"]);
    e["date"]        = text(row["date"]);
    e["description"] = text(row["description"]);
    e["price"]       = integer(row["price"]);
    e["location"]    = text(row["location"]);
    e["event_image"] = text(row["event_image"]);
    e["time"]        = text(row["time"]);
    e["start_date"]  = text(row["start_date"]);
    e["start_time"]  = text(row["start_time"]);
    e["capacity"]    = integer(row["capacity"]);
    e["category"]    = Json::Value(Json::arrayValue);
    e["event_link"]  = frontendLink + "/events/" + e["id"].asString() + "/";

    Json::Value host(Json::objectValue);
    if (!row["host_id"].isNull()) {
        host["id"]          = row["host_id"].as<std::string>();
        host["full_name"]   = text(row["host_full_name"]);
        host["email"]       = text(row["host_email"]);
        host["profile_img"] = text(row["host_profile_img"]);
    }
    e["hosted_by"] = host;
    return e;
}

Task<std::unordered_map<std::string, Json::Value>> tagsFor(DbClientPtr db, std::vector<std::string> eventIds) {
    std::unordered_map<std::string, Json::Value> tags;
    if (eventIds.empty()) co_return tags;

    const auto rows = co_await db->execSqlCoro(
        "SELECT et.event_id, t.id, t.tag_name "
        "FROM event_event_tags et "
        "JOIN event_tags t ON t.id = et.event_tag_id "
        "WHERE et.event_id = ANY($1::uuid[]) "
        "ORDER BY t.tag_name",
        uuidArray(eventIds));

    for (const auto& row : rows) {
        Json::Value tag;
        tag["id"]       = row["id"].as<std::string>();
        tag["tag_name"] = text(row["tag_name"]);
        auto& list = tags[row["event_id"].as<std::string>()];
        if (list.isNull()) list = Json::Value(Json::arrayValue);
        list.append(tag);
    }
    co_return tags;
}

Json::Value event(const Events& event, const Users* host, const std::string& frontendLink) {
    Json::Value e;
    e["id"]          = event.getValueOfId();
    e["event_name"]  = text(event.getEventName());
    e["date"]        = day(event.getDate());
    e["description"] = text(event.getDescription());
    e["price"]       = integer(event.getPrice());
    e["location"]    = text(event.getLocation());
    e["event_image"] = text(event.getEventImage());
    e["time"]        = text(event.getTime());
    e["start_date"]  = day(event.getStartDate());
    e["start_time"]  = text(event.getStartTime());
    e["capacity"]    = integer(event.getCapacity());
    e["category"]    = Json::Value(Json::arrayValue);
    e["event_link"]  = frontendLink + "/events/" + e["id"].asString() + "/";

    Json::Value hostJson(Json::objectValue);
    if (host) {
        hostJson["id"]          = host->getValueOfId();
        hostJson["full_name"]   = text(host->getFullName());
        hostJson["email"]       = text(host->getEmail());
        hostJson["profile_img"] = text(host->getProfileImg());
    }
    e["hosted_by"] = hostJson;
    return e;
}

Task<Json::Value> events(DbClientPtr db, std::vector<Events> list, std::string frontendLink) {
    std::unordered_set<std::string> seen;
    std::vector<std::string> hostIds;
    std::vector<std::string> eventIds;
    for (const auto& item : list) {
        eventIds.push_back(item.getValueOfId());
        if (item.getCreatedBy() && seen.insert(item.getValueOfCreatedBy()).second) hostIds.push_back(item.getValueOfCreatedBy());
    }

    std::unordered_map<std::string, Users> hosts;
    if (!hostIds.empty()) {
        const auto users = co_await drogon::orm::CoroMapper<Users>(db).findBy(
            drogon::orm::Criteria(Users::Cols::_id, drogon::orm::CompareOperator::In, hostIds));
        for (const auto& user : users) hosts.emplace(user.getValueOfId(), user);
    }

    const auto tags = co_await tagsFor(db, eventIds);
    Json::Value result(Json::arrayValue);
    for (const auto& item : list) {
        const auto host = item.getCreatedBy() ? hosts.find(item.getValueOfCreatedBy()) : hosts.end();
        auto e = event(item, host == hosts.end() ? nullptr : &host->second, frontendLink);
        if (const auto it = tags.find(item.getValueOfId()); it != tags.end()) e["category"] = it->second;
        result.append(e);
    }
    co_return result;
}

Task<Json::Value> events(DbClientPtr db, Result rows, std::string frontendLink) {
    Json::Value list(Json::arrayValue);
    std::vector<std::string> ids;
    for (const auto& row : rows) {
        list.append(event(row, frontendLink));
        ids.push_back(row["id"].as<std::string>());
    }

    const auto tags = co_await tagsFor(db, ids);
    for (auto& e : list) {
        if (const auto it = tags.find(e["id"].asString()); it != tags.end()) e["category"] = it->second;
    }
    co_return list;
}

Task<Json::Value> users(DbClientPtr db, std::vector<std::string> ids, std::string frontendLink) {
    Json::Value list(Json::arrayValue);
    if (ids.empty()) co_return list;
    const std::string idArray = uuidArray(ids);

    const auto userRows = co_await db->execSqlCoro(
        "SELECT u.id, u.full_name, u.email, u.profile_img, u.location, u.website, u.instagram, u.facebook, u.twitter, u.bio, "
        "       COALESCE(a.upcoming, 0) AS upcoming_events_count, "
        "       COALESCE(a.past, 0) AS past_events_count, "
        "       COALESCE(c.created, 0) AS created_events_count "
        "FROM users u "
        "LEFT JOIN ( "
        "    SELECT eu.user_id, "
        "           COUNT(*) FILTER (WHERE e.start_date >= (NOW() AT TIME ZONE 'UTC')::date) AS upcoming, "
        "           COUNT(*) FILTER (WHERE e.start_date <  (NOW() AT TIME ZONE 'UTC')::date) AS past "
        "    FROM event_users eu "
        "    JOIN events e ON e.id = eu.event_id "
        "    WHERE eu.user_id = ANY($1::uuid[]) "
        "    GROUP BY eu.user_id "
        ") a ON a.user_id = u.id "
        "LEFT JOIN ( "
        "    SELECT created_by, COUNT(*) AS created "
        "    FROM events "
        "    WHERE created_by = ANY($1::uuid[]) "
        "    GROUP BY created_by "
        ") c ON c.created_by = u.id "
        "WHERE u.id = ANY($1::uuid[])",
        idArray);

    const auto eventRows = co_await db->execSqlCoro(
        "SELECT eu.user_id AS attendee_id, " + EVENT_COLUMNS + " "
        "FROM event_users eu "
        "JOIN events e ON e.id = eu.event_id "
        "LEFT JOIN users h ON h.id = e.created_by "
        "WHERE eu.user_id = ANY($1::uuid[]) "
        "  AND e.start_date >= (NOW() AT TIME ZONE 'UTC')::date "
        "ORDER BY e.created_at DESC, e.id",
        idArray);

    const Json::Value upcoming = co_await events(db, eventRows, frontendLink);
    std::unordered_map<std::string, Json::Value> upcomingByUser;
    for (Json::ArrayIndex i = 0; i < upcoming.size(); i++) {
        auto& userEvents = upcomingByUser[eventRows[i]["attendee_id"].as<std::string>()];
        if (userEvents.isNull()) userEvents = Json::Value(Json::arrayValue);
        userEvents.append(upcoming[i]);
    }

    std::unordered_map<std::string, Json::Value> byId;
    for (const auto& row : userRows) {
        Json::Value u;
        const std::string id = row["id"].as<std::string>();
        u["id"]                    = id;
        u["full_name"]             = text(row["full_name"]);
        u["email"]                 = text(row["email"]);
        u["profile_img"]           = text(row["profile_img"]);
        u["upcoming_events_count"] = integer(row["upcoming_events_count"]);
        u["past_events_count"]     = integer(row["past_events_count"]);
        u["created_events_count"]  = integer(row["created_events_count"]);
        u["upcoming_events"]       = upcomingByUser.count(id) ? upcomingByUser[id] : Json::Value(Json::arrayValue);
        u["location"]              = text(row["location"]);
        u["website"]               = text(row["website"]);
        u["instagram"]             = text(row["instagram"]);
        u["facebook"]              = text(row["facebook"]);
        u["twitter"]               = text(row["twitter"]);
        u["bio"]                   = text(row["bio"]);
        byId[id] = u;
    }

    for (const auto& id : ids) {
        if (const auto it = byId.find(id); it != byId.end()) list.append(it->second);
    }
    co_return list;
}

}
