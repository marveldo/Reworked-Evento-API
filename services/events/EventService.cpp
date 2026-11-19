#include "EventService.hpp"
#include <drogon/orm/Exception.h>
#include "services/serializers/Serializers.hpp"
#include "events/Events.hpp"
#include "utils/DbTransaction.hpp"
#include <drogon/orm/CoroMapper.h>
#include "utils/Errors.hpp"
#include "models/EventEventTags.h"
#include "models/EventUsers.h"
#include "models/Users.h"
#include "models/UserInterests.h"

using drogon::Task;
using drogon_model::evento::EventEventTags;
using drogon_model::evento::EventUsers;
using drogon_model::evento::Events;
using drogon_model::evento::Users;
using drogon_model::evento::UserInterests;
using namespace drogon::orm;

namespace {
    Task<> tagEvent(const DbClientPtr& db, const std::string& eventId, const std::string& tagId) {
        EventEventTags link;
        link.setEventId(eventId);
        link.setEventTagId(tagId);
        co_await CoroMapper<EventEventTags>(db).insert(link);
    }

    std::string containsPattern(const std::string& text) {
        std::string out = "%";
        for (char c : text) {
            if (c == '%' || c == '_' || c == '\\') out += '\\';
            out += c;
        }
        return out + "%";
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
        } else if (tag == "Recommended" && !userId.empty()) {
            const size_t interests =
                co_await CoroMapper<UserInterests>(dbClient_).count(Criteria(UserInterests::Cols::_user_id, userId));
            if (interests > 0) {
                where = "WHERE EXISTS (SELECT 1 FROM event_event_tags x "
                        "JOIN user_interests ui ON ui.event_tag_id = x.event_tag_id "
                        "WHERE x.event_id = e.id AND ui.user_id = $1)";
                param = userId;
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
    const std::string today = trantor::Date::now().toCustomFormattedString("%Y-%m-%d");

    Criteria criteria = Criteria("event_users.user_id", userId) &&
                        (filter.past ? Criteria(Events::Cols::_start_date, CompareOperator::LT, today)
                                     : Criteria(Events::Cols::_start_date, CompareOperator::GE, today));
    if (filter.role == UserEventsFilter::Role::Hosting)
        criteria = criteria && Criteria(Events::Cols::_created_by, userId);
    if (filter.role == UserEventsFilter::Role::Attending)
        criteria = criteria && (Criteria(Events::Cols::_created_by, CompareOperator::NE, userId) ||
                                Criteria(Events::Cols::_created_by, CompareOperator::IsNull));
    if (filter.price == UserEventsFilter::Price::Free)
        criteria = criteria && (Criteria(Events::Cols::_price, 0) || Criteria(Events::Cols::_price, CompareOperator::IsNull));
    if (filter.price == UserEventsFilter::Price::Paid)
        criteria = criteria && Criteria(Events::Cols::_price, CompareOperator::GT, 0);

    EventPage result;
    CoroMapper<Events> counter(dbClient_);
    counter.innerJoin("event_users", "event_users.event_id", "events.id");
    result.count = co_await counter.count(criteria);

    const auto sort = filter.past ? SortOrder::DESC : SortOrder::ASC;
    CoroMapper<Events> events(dbClient_);
    events.innerJoin("event_users", "event_users.event_id", "events.id");
    events.orderBy(Events::Cols::_start_date, sort)
          .orderBy(Events::Cols::_start_time, sort)
          .orderBy(Events::Cols::_id)
          .limit(page.limit)
          .offset(page.offset);
    result.events = co_await serializers::events(dbClient_, co_await events.findBy(criteria), frontendLink_);
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

Task<Events> EventService::getEventById(const std::string& eventId) {
    CoroMapper<Events> events(dbClient_);
   try {
        co_return co_await events.findByPrimaryKey(eventId);
    } catch (const UnexpectedRows&) {
        throw NotFound("Event not found");
    }
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

            EventUsers host;
            host.setEventId(eventId);
            host.setUserId(userId);
            co_await CoroMapper<EventUsers>(tx).insert(host);

            const auto tag = co_await tx->execSqlCoro(
                "INSERT INTO event_tags (tag_name) VALUES ($1) "
                "ON CONFLICT (tag_name) DO UPDATE SET tag_name = EXCLUDED.tag_name RETURNING id",
                input.eventCategory);
            co_await tagEvent(tx, eventId, tag[0]["id"].as<std::string>());
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
            co_await CoroMapper<EventEventTags>(tx).deleteBy(Criteria(EventEventTags::Cols::_event_id, eventId));
            co_await tagEvent(tx, eventId, tag[0]["id"].as<std::string>());
        } catch (...) {
            tx->rollback();
            throw;
        }
    }
    if (!co_await CommitAwaiter(std::move(tx))) throw std::runtime_error("updating the event could not be committed");
    co_return co_await getEvent(eventId);
}

Task<std::optional<std::string>> EventService::hostOf(const std::string& eventId, bool& eventExists) {
    const auto events = co_await CoroMapper<Events>(dbClient_).findBy(Criteria(Events::Cols::_id, eventId));
    eventExists = !events.empty();
    if (events.empty() || !events[0].getCreatedBy()) co_return std::nullopt;
    co_return events[0].getValueOfCreatedBy();
}

Task<AttendeePage> EventService::attendees(const std::string& eventId, const std::string& hostId, const PageInput& page) {
    AttendeePage result;
    const Criteria guests = Criteria(EventUsers::Cols::_event_id, eventId) &&
                            Criteria(EventUsers::Cols::_user_id, CompareOperator::NE, hostId);
    result.count = co_await CoroMapper<EventUsers>(dbClient_).count(guests);

    const auto rows = co_await CoroMapper<EventUsers>(dbClient_)
                          .orderBy(EventUsers::Cols::_registered_at)
                          .orderBy(EventUsers::Cols::_user_id)
                          .limit(page.limit)
                          .offset(page.offset)
                          .findBy(guests);
    for (const auto& row : rows) result.userIds.push_back(row.getValueOfUserId());
    co_return result;
}

Task<RegisterResult> EventService::registerAttendee(const std::string& eventId, const std::string& email) {
    bool exists = false;
    const auto host = co_await hostOf(eventId, exists);
    if (!exists) co_return RegisterResult::EventNotFound;

    const auto users = co_await CoroMapper<Users>(dbClient_).findBy(Criteria(Users::Cols::_email, email));
    if (users.empty()) co_return RegisterResult::UserNotFound;
    if (!host) co_return RegisterResult::HostNotFound;

    const std::string userId = users[0].getValueOfId();
    const auto inserted = co_await dbClient_->execSqlCoro(
        "INSERT INTO event_users (event_id, user_id) VALUES ($1, $2) ON CONFLICT DO NOTHING", eventId, userId);
    if (inserted.affectedRows() == 0) co_return RegisterResult::AlreadyRegistered;

    events_->Publish(ATTENDEE_REGISTERED, AttendeeRegistered{eventId, *host, email});
    co_return RegisterResult::Registered;
}

Task<RegistrationStatus> EventService::checkRegistration(const std::string& eventId, const std::string& email) {
    const auto users = co_await CoroMapper<Users>(dbClient_).findBy(Criteria(Users::Cols::_email, email));
    if (users.empty()) co_return RegistrationStatus{};

    const std::string userId = users[0].getValueOfId();
    const size_t registered = co_await CoroMapper<EventUsers>(dbClient_).count(
        Criteria(EventUsers::Cols::_event_id, eventId) && Criteria(EventUsers::Cols::_user_id, userId));
    co_return RegistrationStatus{registered > 0 ? RegistrationCheck::AlreadyRegistered : RegistrationCheck::Open, userId};
}

Task<bool> EventService::addAttendee(const std::string& eventId, const std::string& userId, DbClientPtr tx) {
    const auto client = tx ? tx : dbClient_;
    const auto inserted = co_await client->execSqlCoro(
        "INSERT INTO event_users (event_id, user_id) VALUES ($1, $2) ON CONFLICT DO NOTHING", eventId, userId);
    co_return inserted.affectedRows() > 0;
}

Task<Json::Value> EventService::categories() {
    const auto rows = co_await dbClient_->execSqlCoro(
        "SELECT t.tag_name, COUNT(x.event_id) AS events_count "
        "FROM event_tags t "
        "LEFT JOIN event_event_tags x ON x.event_tag_id = t.id "
        "WHERE t.tag_name IS NOT NULL AND t.tag_name <> '' AND t.tag_name <> 'Recommended' "
        "GROUP BY t.id, t.tag_name "
        "ORDER BY events_count DESC, t.tag_name");
    Json::Value list(Json::arrayValue);
    for (const auto& row : rows) {
        Json::Value item;
        item["name"]         = row["tag_name"].as<std::string>();
        item["events_count"] = static_cast<Json::Int64>(row["events_count"].as<int64_t>());
        list.append(item);
    }
    co_return list;
}
