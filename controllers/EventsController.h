#pragma once

#include <drogon/HttpController.h>

#include "services/Services.hpp"

#include "Api.hpp"

using namespace drogon;

class EventsController : public drogon::HttpController<EventsController, false>
{
  public:

    explicit EventsController(Services* srv) : services_(srv) {}

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(EventsController::ListEvents,  API_V1 + "/events/", Get, "OptionalJwtMiddleWare");
    ADD_METHOD_TO(EventsController::ListCategories, API_V1 + "/categories/", Get);
    ADD_METHOD_TO(EventsController::CreateEvent, API_V1 + "/events/", Post, "JwtMiddleWare");
    ADD_METHOD_VIA_REGEX(EventsController::GetEvent,         API_V1 + "/events/([^/]+)/?", Get, "OptionalJwtMiddleWare");
    ADD_METHOD_VIA_REGEX(EventsController::UpdateEvent,      API_V1 + "/events/([^/]+)/?", Put, "JwtMiddleWare");
    ADD_METHOD_VIA_REGEX(EventsController::ListAttendees,    API_V1 + "/events/([^/]+)/attendees/?", Get, "JwtMiddleWare");
    ADD_METHOD_VIA_REGEX(EventsController::RegisterAttendee, API_V1 + "/events/([^/]+)/attendees/?", Post, "JwtMiddleWare");
    METHOD_LIST_END

    Task<HttpResponsePtr> ListCategories(HttpRequestPtr req);
    Task<HttpResponsePtr> ListEvents(HttpRequestPtr req);
    Task<HttpResponsePtr> CreateEvent(HttpRequestPtr req);
    Task<HttpResponsePtr> GetEvent(HttpRequestPtr req, std::string id);
    // Host only. Same body as CreateEvent; leave out event_image to keep the current image.
    Task<HttpResponsePtr> UpdateEvent(HttpRequestPtr req, std::string id);
    Task<HttpResponsePtr> ListAttendees(HttpRequestPtr req, std::string id);
    Task<HttpResponsePtr> RegisterAttendee(HttpRequestPtr req, std::string id);

  private:
    Task<HttpResponsePtr> attendeesPage(HttpRequestPtr req, const std::string& eventId, const std::string& hostId);
    Task<HttpResponsePtr> completeRegisteration(HttpRequestPtr req, std::string& id , const RegisterAttendeeInput& input );
    Task<HttpResponsePtr> paymentRequired(const drogon_model::evento::Events& event, const RegisterAttendeeInput& input);

    Services* services_;
};
