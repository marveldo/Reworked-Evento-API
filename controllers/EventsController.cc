#include "EventsController.h"
#include "input/EventInputs.hpp"
#include "input/QueryInputs.hpp"
#include "utils/Pagination.hpp"
#include "utils/RequestBody.hpp"
#include "utils/Media.hpp"
#include <regex>

namespace {
    bool isUuid(const std::string& value) {
        static const std::regex uuid(R"([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})");
        return std::regex_match(value, uuid);
    }

    PageInput pageOf(const HttpRequestPtr& req) {
        return parsePageQuery(req->getParameter("limit"), req->getParameter("offset"));
    }

    const char* EVENT_NOT_FOUND = "No Event matches the given query.";
}

Task<HttpResponsePtr> EventsController::ListEvents(HttpRequestPtr req) {
    const auto page   = pageOf(req);
    const auto userId = req->attributes()->get<std::string>("id");
    // ?search= takes precedence over ?tag=.
    std::string search = req->getParameter("search");
    search.erase(0, search.find_first_not_of(" \t"));
    search.erase(search.find_last_not_of(" \t") + 1);
    if (search.size() > 100) search.resize(100);

    EventPage result;
    if (search.empty()) result = co_await services_->eventService->listEvents(req->getParameter("tag"), userId, page);
    else result = co_await services_->eventService->searchEvents(search, page);
    co_return pagination::response(req, page, result.count, result.events);
}

Task<HttpResponsePtr> EventsController::ListCategories(HttpRequestPtr) {
    co_return res::success(k200OK, "Categories", co_await services_->eventService->categories());
}

Task<HttpResponsePtr> EventsController::CreateEvent(HttpRequestPtr req) {
    if (co_await services_->userService->roleOf(req->attributes()->get<std::string>("id")) != "organizer") {
        co_return res::error(k403Forbidden, "Only organizers can create events. Become an organizer in your settings.");
    }

    const auto body = readBody(req);
    if (body.error) co_return res::validation({*body.error});

    auto parsed = parseCreateEvent(body);
    if (auto* errors = std::get_if<std::vector<FieldError>>(&parsed)) co_return res::validation(*errors);
    const auto& input = std::get<CreateEventInput>(parsed);

    std::optional<std::string> imageUrl;
    if (input.eventImage) {
        if (!co_await services_->cloudinaryService->exists(*input.eventImage)) {
            co_return res::validation("event_image", "no uploaded photo found with this public_id");
        }
        imageUrl = media::photoUrl(*input.eventImage);
    }

    const auto userId = req->attributes()->get<std::string>("id");
    co_return res::success(k201Created, "Event created",
                           co_await services_->eventService->createEvent(input, userId, imageUrl));
}

Task<HttpResponsePtr> EventsController::GetEvent(HttpRequestPtr, std::string id) {
    if (!isUuid(id)) co_return res::notFound(EVENT_NOT_FOUND);
    const auto event = co_await services_->eventService->getEvent(id);
    if (!event) co_return res::notFound(EVENT_NOT_FOUND);
    co_return res::success(k200OK, "Event Fetched Successfully", *event);
}

Task<HttpResponsePtr> EventsController::UpdateEvent(HttpRequestPtr req, std::string id) {
    if (!isUuid(id)) co_return res::notFound(EVENT_NOT_FOUND);

    bool exists = false;
    const auto host = co_await services_->eventService->hostOf(id, exists);
    if (!exists) co_return res::notFound(EVENT_NOT_FOUND);
    const auto userId = req->attributes()->get<std::string>("id");
    if (!host || *host != userId) co_return res::error(k403Forbidden, "Only the host can edit this event.");

    const auto body = readBody(req);
    if (body.error) co_return res::validation({*body.error});
    auto parsed = parseCreateEvent(body);
    if (auto* errors = std::get_if<std::vector<FieldError>>(&parsed)) co_return res::validation(*errors);
    const auto& input = std::get<CreateEventInput>(parsed);

    std::optional<std::string> imageUrl;
    if (input.eventImage) {
        if (!co_await services_->cloudinaryService->exists(*input.eventImage)) {
            co_return res::validation("event_image", "no uploaded photo found with this public_id");
        }
        imageUrl = media::photoUrl(*input.eventImage);
    }

    const auto event = co_await services_->eventService->updateEvent(id, input, imageUrl);
    if (!event) co_return res::notFound(EVENT_NOT_FOUND);
    co_return res::success(k200OK, "Event updated", *event);
}

Task<HttpResponsePtr> EventsController::ListAttendees(HttpRequestPtr req, std::string id) {
    if (!isUuid(id)) co_return res::notFound(EVENT_NOT_FOUND);

    bool exists = false;
    const auto host = co_await services_->eventService->hostOf(id, exists);
    if (!exists) co_return res::notFound(EVENT_NOT_FOUND);
    if (!host) co_return res::success(k404NotFound, "Detail Not Found");
    co_return co_await attendeesPage(req, id, *host);
}

Task<HttpResponsePtr> EventsController::RegisterAttendee(HttpRequestPtr req, std::string id) {
    if (!isUuid(id)) co_return res::notFound(EVENT_NOT_FOUND);

    const auto body = readBody(req);
    if (body.error) co_return res::validation({*body.error});
    auto parsed = parseRegisterAttendee(body.fields);
    if (auto* errors = std::get_if<std::vector<FieldError>>(&parsed)) co_return res::validation(*errors);
    const auto& input = std::get<RegisterAttendeeInput>(parsed);
    const auto event_details = co_await services_->eventService->getEventById(id);

    if (event_details.getValueOfPrice() <= 0) co_return co_await completeRegisteration(req, id, input);
    co_return co_await paymentRequired(event_details, input);
}

Task<HttpResponsePtr> EventsController::paymentRequired(const drogon_model::evento::Events& event, const RegisterAttendeeInput& input) {
    if (!event.getCreatedBy()) co_return res::success(k404NotFound, "Detail Not Found");

    const auto registration = co_await services_->eventService->checkRegistration(event.getValueOfId(), input.email);
    switch (registration.check) {
        case RegistrationCheck::UserNotFound:      co_return res::success(k404NotFound, "Detail Not Found");
        case RegistrationCheck::AlreadyRegistered: co_return res::success(k400BadRequest, "User Already registered");
        case RegistrationCheck::Open:              break;
    }

    const int64_t amount = static_cast<int64_t>(event.getValueOfPrice()) * 100;
    auto& orders = *services_->orderService;

    std::optional<Orders> order = co_await orders.findPending(event.getValueOfId(), registration.userId);
    if (order && order->getValueOfAmount() != amount) {
        co_await orders.markFailed(order->getValueOfReference(), "abandoned");
        order.reset();
    }
    const bool created = !order;
    if (created) order = co_await orders.create({registration.userId, event.getValueOfId(), input.email, amount});

    const std::vector<Orders> one{*order};
    const auto items = co_await orders.withEvents(one);

    Json::Value data;
    data["status"]   = "payment_required";
    data["event_id"] = event.getValueOfId();
    data["order"]    = items[0];
    co_return res::success(created ? k201Created : k200OK, created ? "Order created" : "You already have a pending order", data);
}

Task<HttpResponsePtr> EventsController::completeRegisteration(HttpRequestPtr req, std::string& id , const RegisterAttendeeInput& input ){
    switch (co_await services_->eventService->registerAttendee(id, input.email)) {
        case RegisterResult::EventNotFound:     co_return res::notFound(EVENT_NOT_FOUND);
        case RegisterResult::UserNotFound:
        case RegisterResult::HostNotFound:      co_return res::success(k404NotFound, "Detail Not Found");
        case RegisterResult::AlreadyRegistered: co_return res::success(k400BadRequest, "User Already registered");
        case RegisterResult::Registered:        break;
    }

    Json::Value data;
    data["status"]   = "registered";
    data["event_id"] = id;
    co_return res::success(k201Created, "Registered", data);
}

Task<HttpResponsePtr> EventsController::attendeesPage(HttpRequestPtr req, const std::string& eventId, const std::string& hostId) {
    const auto page   = pageOf(req);
    const auto result = co_await services_->eventService->attendees(eventId, hostId, page);
    co_return pagination::response(req, page, result.count, co_await services_->userService->usersJson(result.userIds));
}
