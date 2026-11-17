#include "EventService.hpp"
#include <drogon/orm/Exception.h>
#include "services/serializers/Serializers.hpp"
#include "events/Events.hpp"
#include "utils/DbTransaction.hpp"

using drogon::Task;

namespace {
    std::string containsPattern(const std::string& text) {
        std::string out = "%";
        for (char c : text) {
            if (c == '%' || c == '_' || c == '\\') out += '\\';
            out += c;
        }
        return out + "%";
    }

    std::string cityOf(const std::string& location) {
        const auto comma = location.find(',');
        if (comma == std::string::npos) return "";
        std::string city = location.substr(comma + 1);
        city = city.substr(0, city.find(','));
        const auto start = city.find_first_not_of(" \t");
        const auto end   = city.find_last_not_of(" \t");
        return start == std::string::npos ? "" : city.substr(start, end - start + 1);
    }

    std::string eventsQuery(const std::string& where, const std::string& limitParam, const std::string& offsetParam) {
        return "SELECT " + serializers::EVENT_COLUMNS + " "
               "FROM events e "
               "LEFT JOIN users h ON h.id = e.created_by " +
               where + " "
               "ORDER BY e.created_at DESC, e.id "
               "LIMIT " + limitParam + " OFFSET " + offsetParam;
    }
}

Task<EventPage> EventService::listEvents(const std::string& tag, const std::string& userId, const PageInput& page) {
    std::string where;
    std::optional<std::string> param;

    if (!tag.empty()) {
        const auto tags = co_await dbClient_->execSqlCoro(
            "SELECT id FROM event_tags WHERE tag_name ILIKE $1 ESCAPE '\\' ORDER BY id LIMIT 1", containsPattern(tag));
        if (!tags.empty()) {
            where = "WHERE EXISTS (SELECT 1 FROM event_event_tags x WHERE x.event_id = e.id AND x.event_tag_id = $1)";
            param = tags[0]["id"].as<std::string>();
        } else if (tag == "Recommended") {
            const auto users = co_await dbClient_->execSqlCoro("SELECT location FROM users WHERE id = $1", userId);
            const std::string location = users.empty() || users[0]["location"].isNull() ? "" : users[0]["location"].as<std::string>();
            if (const auto city = cityOf(location); !city.empty()) {
                where = "WHERE e.location ILIKE $1 ESCAPE '\\'";
                param = containsPattern(city);
            }
        }
    }

    const auto limit  = static_cast<int64_t>(page.limit);
    const auto offset = static_cast<int64_t>(page.offset);
    EventPage result;

    if (param) {
        const auto count = co_await dbClient_->execSqlCoro("SELECT COUNT(*) AS n FROM events e " + where, *param);
        result.count = count[0]["n"].as<size_t>();
        const auto rows = co_await dbClient_->execSqlCoro(eventsQuery(where, "$2", "$3"), *param, limit, offset);
        result.events = co_await serializers::events(dbClient_, rows, frontendLink_);
    } else {
        const auto count = co_await dbClient_->execSqlCoro("SELECT COUNT(*) AS n FROM events e");
        result.count = count[0]["n"].as<size_t>();
        const auto rows = co_await dbClient_->execSqlCoro(eventsQuery("", "$1", "$2"), limit, offset);
        result.events = co_await serializers::events(dbClient_, rows, frontendLink_);
    }
    co_return result;
}

Task<EventPage> EventService::searchEvents(const std::string& text, const PageInput& page) {
    static const std::string where =
        "WHERE e.event_name ILIKE $1 ESCAPE '\\' "
        "OR e.description ILIKE $1 ESCAPE '\\' "
        "OR e.location ILIKE $1 ESCAPE '\\' "
        "OR h.full_name ILIKE $1 ESCAPE '\\' "
        "OR EXISTS (SELECT 1 FROM event_event_tags x JOIN event_tags t ON t.id = x.event_tag_id "
        "WHERE x.event_id = e.id AND t.tag_name ILIKE $1 ESCAPE '\\')";
    const auto pattern = containsPattern(text);

    EventPage result;
    const auto count = co_await dbClient_->execSqlCoro(
        "SELECT COUNT(*) AS n FROM events e LEFT JOIN users h ON h.id = e.created_by " + where, pattern);
    result.count = count[0]["n"].as<size_t>();

    const auto rows = co_await dbClient_->execSqlCoro(
        "SELECT " + serializers::EVENT_COLUMNS + " "
        "FROM events e "
        "LEFT JOIN users h ON h.id = e.created_by " +
        where + " "
        "ORDER BY (e.event_name ILIKE $1 ESCAPE '\\') DESC, e.created_at DESC, e.id "
        "LIMIT $2 OFFSET $3",
        pattern, static_cast<int64_t>(page.limit), static_cast<int64_t>(page.offset));
    result.events = co_await serializers::events(dbClient_, rows, frontendLink_);
    co_return result;
}

Task<EventPage> EventService::userEvents(const std::string& userId, const UserEventsFilter& filter, const PageInput& page) {
    // Only fixed SQL fragments are added here; the one user-supplied value is bound as $1.
    std::string where = "WHERE EXISTS (SELECT 1 FROM event_users eu WHERE eu.event_id = e.id AND eu.user_id = $1) ";
    where += filter.past ? "AND e.start_date < (NOW() AT TIME ZONE 'UTC')::date "
                         : "AND e.start_date >= (NOW() AT TIME ZONE 'UTC')::date ";
    if (filter.role == UserEventsFilter::Role::Hosting) where += "AND e.created_by = $1 ";
    if (filter.role == UserEventsFilter::Role::Attending) where += "AND e.created_by IS DISTINCT FROM $1 ";
    if (filter.price == UserEventsFilter::Price::Free) where += "AND COALESCE(e.price, 0) = 0 ";
    if (filter.price == UserEventsFilter::Price::Paid) where += "AND e.price > 0 ";
    const std::string order = filter.past ? "ORDER BY e.start_date DESC, e.start_time DESC, e.id "
                                          : "ORDER BY e.start_date ASC, e.start_time ASC, e.id ";

    EventPage result;
    const auto count = co_await dbClient_->execSqlCoro("SELECT COUNT(*) AS n FROM events e " + where, userId);
    result.count = count[0]["n"].as<size_t>();

    const auto rows = co_await dbClient_->execSqlCoro(
        "SELECT " + serializers::EVENT_COLUMNS + " "
        "FROM events e "
        "LEFT JOIN users h ON h.id = e.created_by " +
        where + order +
        "LIMIT $2 OFFSET $3",
        userId, static_cast<int64_t>(page.limit), static_cast<int64_t>(page.offset));
    result.events = co_await serializers::events(dbClient_, rows, frontendLink_);
    co_return result;
}

Task<std::optional<Json::Value>> EventService::getEvent(const std::string& id) {
    const auto rows = co_await dbClient_->execSqlCoro(
        "SELECT " + serializers::EVENT_COLUMNS + " "
        "FROM events e "
        "LEFT JOIN users h ON h.id = e.created_by "
        "WHERE e.id = $1",
        id);
    if (rows.empty()) co_return std::nullopt;
    const auto events = co_await serializers::events(dbClient_, rows, frontendLink_);
    co_return events[0];
}

Task<Json::Value> EventService::createEvent(const CreateEventInput& input, const std::string& userId,
                                            const std::optional<std::string>& eventImageUrl) {
    std::string eventId;
    std::shared_ptr<drogon::orm::Transaction> tx = co_await dbClient_->newTransactionCoro();
    {
        try {
            const auto inserted = co_await tx->execSqlCoro(
                "INSERT INTO events (event_name, date, description, start_date, start_time, \"time\", price, location, "
                "event_image, capacity, created_by) "
                "VALUES ($1, $2::date, $3, $4::date, $5::time, $6::time, $7, $8, $9, $10, $11) RETURNING id",
                input.eventName, input.date, input.description, input.startDate, input.startTime, input.time,
                static_cast<int32_t>(input.price.value_or(0)), input.location, eventImageUrl,
                static_cast<int32_t>(input.capacity.value_or(0)), userId);
            eventId = inserted[0]["id"].as<std::string>();

            co_await tx->execSqlCoro("INSERT INTO event_users (event_id, user_id) VALUES ($1, $2)", eventId, userId);

            const auto tag = co_await tx->execSqlCoro(
                "INSERT INTO event_tags (tag_name) VALUES ($1) "
                "ON CONFLICT (tag_name) DO UPDATE SET tag_name = EXCLUDED.tag_name RETURNING id",
                input.eventCategory);
            co_await tx->execSqlCoro("INSERT INTO event_event_tags (event_id, event_tag_id) VALUES ($1, $2)",
                                     eventId, tag[0]["id"].as<std::string>());
        } catch (...) {
            tx->rollback();
            throw;
        }
    }
    if (!co_await CommitAwaiter(std::move(tx))) throw std::runtime_error("creating the event could not be committed");

    auto event = co_await getEvent(eventId);
    if (!event) throw std::runtime_error("event " + eventId + " disappeared right after it was created");
    co_return *event;
}

Task<std::optional<Json::Value>> EventService::updateEvent(const std::string& eventId, const CreateEventInput& input,
                                                          const std::optional<std::string>& eventImageUrl) {
    std::shared_ptr<drogon::orm::Transaction> tx = co_await dbClient_->newTransactionCoro();
    {
        try {
            const auto updated = co_await tx->execSqlCoro(
                "UPDATE events SET event_name = $1, date = $2::date, description = $3, start_date = $4::date, "
                "start_time = $5::time, \"time\" = $6::time, price = $7, location = $8, "
                "event_image = COALESCE($9, event_image), capacity = $10 "
                "WHERE id = $11 RETURNING id",
                input.eventName, input.date, input.description, input.startDate, input.startTime, input.time,
                static_cast<int32_t>(input.price.value_or(0)), input.location, eventImageUrl,
                static_cast<int32_t>(input.capacity.value_or(0)), eventId);
            if (updated.empty()) {
                tx->rollback();
                co_return std::nullopt;
            }

            const auto tag = co_await tx->execSqlCoro(
                "INSERT INTO event_tags (tag_name) VALUES ($1) "
                "ON CONFLICT (tag_name) DO UPDATE SET tag_name = EXCLUDED.tag_name RETURNING id",
                input.eventCategory);
            co_await tx->execSqlCoro("DELETE FROM event_event_tags WHERE event_id = $1", eventId);
            co_await tx->execSqlCoro("INSERT INTO event_event_tags (event_id, event_tag_id) VALUES ($1, $2)",
                                     eventId, tag[0]["id"].as<std::string>());
        } catch (...) {
            tx->rollback();
            throw;
        }
    }
    if (!co_await CommitAwaiter(std::move(tx))) throw std::runtime_error("updating the event could not be committed");
    co_return co_await getEvent(eventId);
}

Task<std::optional<std::string>> EventService::hostOf(const std::string& eventId, bool& eventExists) {
    const auto rows = co_await dbClient_->execSqlCoro("SELECT created_by FROM events WHERE id = $1", eventId);
    eventExists = !rows.empty();
    if (rows.empty() || rows[0]["created_by"].isNull()) co_return std::nullopt;
    co_return rows[0]["created_by"].as<std::string>();
}

Task<AttendeePage> EventService::attendees(const std::string& eventId, const std::string& hostId, const PageInput& page) {
    AttendeePage result;
    const auto count = co_await dbClient_->execSqlCoro(
        "SELECT COUNT(*) AS n FROM event_users WHERE event_id = $1 AND user_id <> $2", eventId, hostId);
    result.count = count[0]["n"].as<size_t>();

    const auto rows = co_await dbClient_->execSqlCoro(
        "SELECT user_id FROM event_users WHERE event_id = $1 AND user_id <> $2 "
        "ORDER BY registered_at, user_id LIMIT $3 OFFSET $4",
        eventId, hostId, static_cast<int64_t>(page.limit), static_cast<int64_t>(page.offset));
    for (const auto& row : rows) result.userIds.push_back(row["user_id"].as<std::string>());
    co_return result;
}

Task<RegisterResult> EventService::registerAttendee(const std::string& eventId, const std::string& email) {
    bool exists = false;
    const auto host = co_await hostOf(eventId, exists);
    if (!exists) co_return RegisterResult::EventNotFound;

    const auto users = co_await dbClient_->execSqlCoro("SELECT id FROM users WHERE email = $1", email);
    if (users.empty()) co_return RegisterResult::UserNotFound;
    if (!host) co_return RegisterResult::HostNotFound;

    const std::string userId = users[0]["id"].as<std::string>();
    const auto inserted = co_await dbClient_->execSqlCoro(
        "INSERT INTO event_users (event_id, user_id) VALUES ($1, $2) ON CONFLICT DO NOTHING", eventId, userId);
    if (inserted.affectedRows() == 0) co_return RegisterResult::AlreadyRegistered;

    events_->Publish(ATTENDEE_REGISTERED, AttendeeRegistered{eventId, *host, email});
    co_return RegisterResult::Registered;
}
