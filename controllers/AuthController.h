#pragma once

#include <drogon/HttpController.h>

#include "services/Services.hpp"

#include "Api.hpp"

using namespace drogon;

// /api/v1/auth/... (Django: users/urls.py). Paths keep their trailing slash, like the Django URLs.
class AuthController : public drogon::HttpController<AuthController, false>
{
  public:

    explicit AuthController(Services* srv) : services_(srv) {}

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AuthController::Home,     API_V1 + "/auth/", Get, "JwtMiddleWare");
    ADD_METHOD_TO(AuthController::Register, API_V1 + "/auth/register/", Post, "SignupRateLimit");
    ADD_METHOD_TO(AuthController::Login,    API_V1 + "/auth/login/", Post, "LoginRateLimit", "DeviceInfoMiddleware");
    ADD_METHOD_TO(AuthController::Refresh,  API_V1 + "/auth/refresh/", Post, "RefreshRateLimit");
    ADD_METHOD_TO(AuthController::Google,   API_V1 + "/auth/google/", Post, "SignupRateLimit", "DeviceInfoMiddleware");
    ADD_METHOD_TO(AuthController::Logout,   API_V1 + "/auth/logout/", Post);
    ADD_METHOD_TO(AuthController::SocketToken, API_V1 + "/auth/socket-token/", Post, "JwtMiddleWare");
    METHOD_LIST_END

    // Checks a token is still good (the frontend's TestToken).
    Task<HttpResponsePtr> Home(HttpRequestPtr req);
    Task<HttpResponsePtr> Register(HttpRequestPtr req);
    // Email + password. Starts a device session: the response has the tokens and data.device_id.
    Task<HttpResponsePtr> Login(HttpRequestPtr req);
    // {"refresh", "device_id"}: new access and refresh tokens; the old refresh token stops working.
    Task<HttpResponsePtr> Refresh(HttpRequestPtr req);
    // {"access_token": <Google ID token>}: logs in, making the account the first time.
    Task<HttpResponsePtr> Google(HttpRequestPtr req);
    // {"device_id", "refresh_token"}: ends that device's session.
    Task<HttpResponsePtr> Logout(HttpRequestPtr req);
    Task<HttpResponsePtr> SocketToken(HttpRequestPtr req);

  private:
    // The login / Google response: tokens plus the user with their device_id.
    Task<HttpResponsePtr> startSession(HttpRequestPtr req, const drogon_model::evento::Users& user, HttpStatusCode status);

    Services* services_;
};
