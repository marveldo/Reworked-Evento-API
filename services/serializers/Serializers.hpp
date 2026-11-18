#pragma once
#include <drogon/orm/DbClient.h>
#include <drogon/orm/Result.h>
#include <drogon/orm/Row.h>
#include <drogon/utils/coroutine.h>
#include <json/json.h>
#include <string>
#include <unordered_map>
#include <vector>
#include "models/Events.h"
#include "models/Users.h"

namespace serializers {

inline const std::string EVENT_COLUMNS =
    "e.id, e.event_name, e.date::text AS date, e.description, e.price, e.location, e.event_image, "
    "e.\"time\"::text AS \"time\", e.start_date::text AS start_date, e.start_time::text AS start_time, e.capacity, "
    "h.id AS host_id, h.full_name AS host_full_name, h.email AS host_email, h.profile_img AS host_profile_img";

std::string uuidArray(const std::vector<std::string>& ids);

Json::Value event(const drogon::orm::Row& row, const std::string& frontendLink);

drogon::Task<std::unordered_map<std::string, Json::Value>> tagsFor(drogon::orm::DbClientPtr db, std::vector<std::string> eventIds);

drogon::Task<Json::Value> events(drogon::orm::DbClientPtr db, drogon::orm::Result rows, std::string frontendLink);

Json::Value event(const drogon_model::evento::Events& event, const drogon_model::evento::Users* host, const std::string& frontendLink);

drogon::Task<Json::Value> events(drogon::orm::DbClientPtr db, std::vector<drogon_model::evento::Events> list, std::string frontendLink);

drogon::Task<Json::Value> users(drogon::orm::DbClientPtr db, std::vector<std::string> ids, std::string frontendLink);

}
