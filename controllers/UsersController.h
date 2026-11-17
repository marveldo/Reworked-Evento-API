#pragma once

#include <drogon/HttpController.h>

#include "services/Services.hpp"

#include "Api.hpp"

using namespace drogon;

class UsersController : public drogon::HttpController<UsersController, false>
{
  public:

    explicit UsersController(Services* srv) : services_(srv) {}

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(UsersController::ListUsers,         API_V1 + "/auth/users/", Get, "JwtMiddleWare");
    ADD_METHOD_TO(UsersController::GetCurrentUser,    API_V1 + "/auth/current-user/", Get, "JwtMiddleWare");
    ADD_METHOD_TO(UsersController::UpdateCurrentUser, API_V1 + "/auth/current-user/", Put, "JwtMiddleWare");
    ADD_METHOD_TO(UsersController::DeleteCurrentUser, API_V1 + "/auth/current-user/", Delete, "JwtMiddleWare");
    ADD_METHOD_TO(UsersController::ListMyEvents,      API_V1 + "/auth/current-user/events/", Get, "JwtMiddleWare");
    METHOD_LIST_END

    Task<HttpResponsePtr> ListUsers(HttpRequestPtr req);
    Task<HttpResponsePtr> GetCurrentUser(HttpRequestPtr req);
    Task<HttpResponsePtr> UpdateCurrentUser(HttpRequestPtr req);
    Task<HttpResponsePtr> DeleteCurrentUser(HttpRequestPtr req);
    // Events the user hosts or registered for. ?when=upcoming|past, ?role=hosting|attending,
    // ?price=free|paid (leave a filter out for both), ?limit=, ?offset=.
    Task<HttpResponsePtr> ListMyEvents(HttpRequestPtr req);

  private:
    Services* services_;
};
