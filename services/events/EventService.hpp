#pragma once
#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>
#include "models/Events.h"
#include <json/json.h>
#include <optional>
#include <string>
#include <vector>
#include "input/EventInputs.hpp"
#include "input/QueryInputs.hpp"
#include "events/EventBus.hpp"

struct EventPage {
    size_t count = 0;
    Json::Value events = Json::Value(Json::arrayValue);
};

struct AttendeePage {
    size_t count = 0;
    std::vector<std::string> userIds;
};

struct UserEventsFilter {
    enum class Role { Any, Hosting, Attending };
    enum class Price { Any, Free, Paid };
    bool past = false;   // false: today and later, true: before today (UTC), like the profile counts
    Role role = Role::Any;
    Price price = Price::Any;
};

enum class RegisterResult { Registered, AlreadyRegistered, EventNotFound, UserNotFound, HostNotFound };
enum class RegistrationCheck { Open, AlreadyRegistered, UserNotFound };

struct RegistrationStatus {
    RegistrationCheck check = RegistrationCheck::UserNotFound;
    std::string userId;
};

class EventService {
    public:
        EventService(drogon::orm::DbClientPtr dbClient, EventBus* events, std::string frontendLink)
            : dbClient_(std::move(dbClient)), events_(events), frontendLink_(std::move(frontendLink)) {}

        drogon::Task<EventPage> listEvents(const std::string& tag, const std::string& userId, const PageInput& page);

        // Events whose name, description, location, host name or category contains `text`
        // (case-insensitive). Events whose name matches come first.
        drogon::Task<EventPage> searchEvents(const std::string& text, const PageInput& page);

        // Events the user hosts or is registered for. Upcoming ones soonest first, past ones latest first.
        drogon::Task<EventPage> userEvents(const std::string& userId, const UserEventsFilter& filter, const PageInput& page);

        drogon::Task<std::optional<Json::Value>> getEvent(const std::string& id);

        drogon::Task<Json::Value> createEvent(const CreateEventInput& input, const std::string& userId,
                                              const std::optional<std::string>& eventImageUrl);

        // Replaces the event's details and category. A null eventImageUrl keeps the current image.
        // Returns the updated event, or nullopt if it no longer exists.
        drogon::Task<std::optional<Json::Value>> updateEvent(const std::string& eventId, const CreateEventInput& input,
                                                             const std::optional<std::string>& eventImageUrl);

        drogon::Task<std::optional<std::string>> hostOf(const std::string& eventId, bool& eventExists);

        drogon::Task<AttendeePage> attendees(const std::string& eventId, const std::string& hostId, const PageInput& page);

        drogon::Task<RegisterResult> registerAttendee(const std::string& eventId, const std::string& email);

        drogon::Task<RegistrationStatus> checkRegistration(const std::string& eventId, const std::string& email);

        drogon::Task<bool> addAttendee(const std::string& eventId, const std::string& userId, drogon::orm::DbClientPtr tx = nullptr);

        drogon::Task<drogon_model::evento::Events> getEventById(const std::string& eventId );

    private:
        drogon::orm::DbClientPtr dbClient_;
        EventBus* events_;
        std::string frontendLink_;
};
