#pragma once

#include <drogon/HttpController.h>

#include "services/Services.hpp"

#include "Api.hpp"

using namespace drogon;

class NotificationsController : public drogon::HttpController<NotificationsController, false>
{
  public:

    explicit NotificationsController(Services* srv) : services_(srv) {}

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(NotificationsController::ListNotifications, API_V1 + "/notifications/", Get, "JwtMiddleWare");
    METHOD_LIST_END

    // The logged-in user's notifications, newest first. Paginated with ?limit= and ?offset=.
    Task<HttpResponsePtr> ListNotifications(HttpRequestPtr req);

  private:
    Services* services_;
};
