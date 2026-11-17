#include "AuthController.h"
#include "input/UserInputs.hpp"
#include "utils/RequestBody.hpp"
#include "utils/Errors.hpp"

namespace {
    std::string attribute(const HttpRequestPtr& req, const char* name) {
        return req->attributes()->find(name) ? req->attributes()->get<std::string>(name) : "";
    }

    bool isUuid(const std::string& value) {
        static const std::regex uuid(R"([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})");
        return std::regex_match(value, uuid);
    }
}

Task<HttpResponsePtr> AuthController::Home(HttpRequestPtr) {
    co_return res::success(k200OK, "welcome");
}

Task<HttpResponsePtr> AuthController::Register(HttpRequestPtr req) {
    const auto body = readBody(req);
    if (body.error) co_return res::validation({*body.error});

    auto parsed = parseRegister(body.fields);
    if (auto* errors = std::get_if<std::vector<FieldError>>(&parsed)) co_return res::validation(*errors);
    const auto& input = std::get<RegisterInput>(parsed);

    const auto user = co_await services_->userService->createUser(input);   // the welcome email is queued
    co_return res::success(k201Created, "Registeration Successful",
                           co_await services_->userService->userJson(user.getValueOfId()));
}

Task<HttpResponsePtr> AuthController::Login(HttpRequestPtr req) {
    const auto body = readBody(req);
    if (body.error) co_return res::validation({*body.error});

    auto parsed = parseLogin(body.fields);
    if (auto* errors = std::get_if<std::vector<FieldError>>(&parsed)) co_return res::validation(*errors);
    const auto& input = std::get<LoginInput>(parsed);

    const auto user = co_await services_->userService->findByEmail(input.email);
    if (!user || !co_await services_->userService->checkPassword(*user, input.password)) {
        Json::Value body401;
        body401["status_code"] = 401;
        body401["status"]      = 401;
        body401["message"]     = "Not Authorized";
        co_return res::json(k401Unauthorized, body401);
    }
    co_return co_await startSession(req, *user, k200OK);
}

Task<HttpResponsePtr> AuthController::Refresh(HttpRequestPtr req) {
    const auto body = readBody(req);
    if (body.error) co_return res::validation({*body.error});

    auto parsed = parseRefresh(body.fields);
    if (auto* errors = std::get_if<std::vector<FieldError>>(&parsed)) co_return res::validation(*errors);
    const auto& input = std::get<RefreshInput>(parsed);

    const auto session = co_await services_->deviceService->refreshSession(input.refresh);
    if (!session) {
        auto resp = res::unauthorized("Token is invalid or expired");
        auto json = *resp->getJsonObject();
        json["code"] = "token_not_valid";
        co_return res::json(k401Unauthorized, json);
    }

    Json::Value user = co_await services_->userService->userJson(session->userId);
    user["device_id"] = session->deviceId;
    co_return res::success(k200OK, "Refresh Successful", user, res::Tokens{session->accessToken, session->refreshToken, session->accessexpirytime , session->refreshexpirytime});
}

Task<HttpResponsePtr> AuthController::Google(HttpRequestPtr req) {
    const auto body = readBody(req);
    if (body.error) co_return res::validation({*body.error});

    auto parsed = parseGoogleSignin(body.fields);
    if (auto* errors = std::get_if<std::vector<FieldError>>(&parsed)) co_return res::validation(*errors);
    const auto& input = std::get<GoogleSigninInput>(parsed);

    const auto identity = co_await services_->googleAuthService->verifyIdToken(input.idToken);
    if (!identity) co_return res::validation("access_token", "Something is wrong with the token");

    const std::string& clientId = getSettings()->google.clientId;
    if (clientId.empty() || identity->audience != clientId) {
        co_return res::unauthorized("could not verify user");
    }

    std::string fullName = identity->givenName;
    if (!identity->familyName.empty()) fullName += (fullName.empty() ? "" : " ") + identity->familyName;
    if (fullName.empty()) fullName = identity->name;

    std::string email = identity->email;
    std::transform(email.begin(), email.end(), email.begin(), [](unsigned char c) { return std::tolower(c); });

    const auto [user, created] = co_await services_->userService->findOrCreateSocialUser(
        SocialProfile{email, fullName, identity->picture, "google"});
    co_return co_await startSession(req, user, created ? k201Created : k200OK);
}

Task<HttpResponsePtr> AuthController::Logout(HttpRequestPtr req) {
    const auto body = readBody(req);
    if (body.error) co_return res::validation({*body.error});

    auto parsed = parseLogout(body.fields);
    if (auto* errors = std::get_if<std::vector<FieldError>>(&parsed)) co_return res::validation(*errors);
    const auto& input = std::get<LogoutInput>(parsed);

    std::shared_ptr<Claims> claims;
    try {
        claims = services_->jwtService->verifyRefreshToken(input.refreshToken);
    } catch (const std::exception&) {
    }
    if (!claims) co_return res::validation("non_field_errors", "Token has been blacklisted");

    // The refresh token's own session ends. The device_id sent with it ends too when it's another
    // device of the same user (the old frontend always sent its own).
    co_await services_->deviceService->endSession(claims->deviceId, claims->userId);
    if (input.deviceId != claims->deviceId && isUuid(input.deviceId)) {
        co_await services_->deviceService->endSession(input.deviceId, claims->userId);
    }
    co_return res::success(k200OK, "Token blacklisted");
}

Task<HttpResponsePtr> AuthController::startSession(HttpRequestPtr req, const drogon_model::evento::Users& user, HttpStatusCode status) {
    const DeviceDetailsInput details{
        attribute(req, "ip_address"),
        attribute(req, "device_name"),
        attribute(req, "device_os"),
        attribute(req, "device_type"),
    };
    const DeviceSession session = co_await services_->deviceService->startSession(user, details);   // the location lookup is queued
    co_await services_->userService->setLastLogin(user.getValueOfId());

    Json::Value data = co_await services_->userService->userJson(user.getValueOfId());
    data["device_id"] = session.deviceId;
    co_return res::success(status, "Login Successful", data, res::Tokens{session.accessToken, session.refreshToken, session.accessexpirytime, session.refreshexpirytime});
}

Task<HttpResponsePtr> AuthController::SocketToken(HttpRequestPtr req) {
    const Claims claims{attribute(req, "id"), "", attribute(req, "email"), attribute(req, "device_id")};
    const Token token = services_->jwtService->generateSocketToken(claims);

    Json::Value data;
    data["token"]      = token.token;
    data["expires_at"] = token.expiry;
    co_return res::success(k200OK, "Socket token created", data);
}
