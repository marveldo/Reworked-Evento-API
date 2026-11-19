#include "JwtMiddleware.hpp"
#include "controllers/Api.hpp"
#include <trantor/utils/Logger.h>

using namespace drogon;

namespace {
    const std::string TOKEN_PREFIX = "Bearer ";
}

std::string JwtMiddleWare::bearerToken(const HttpRequestPtr& req) {
    const std::string& authorization = req->getHeader("Authorization");
    if (authorization.size() <= TOKEN_PREFIX.size() || authorization.compare(0, TOKEN_PREFIX.size(), TOKEN_PREFIX) != 0) {
        return "";
    }
    std::string token = authorization.substr(TOKEN_PREFIX.size());
    const auto end = token.find_last_not_of(" \t\r\n");
    token.erase(end == std::string::npos ? 0 : end + 1);
    return token;
}

Task<std::shared_ptr<Claims>> JwtMiddleWare::authenticate(Services* services, std::string token) {
    if (token.empty()) co_return nullptr;

    std::shared_ptr<Claims> claims;
    try {
        claims = services->jwtService->verifyAccessToken(token);
    } catch (const std::exception& e) {
        LOG_DEBUG << "rejected access token: " << e.what();
        co_return nullptr;
    }
    if (!co_await services->deviceService->isCurrentAccessToken(claims->deviceId, claims->userId, token)) co_return nullptr;
    co_return claims;
}

Task<HttpResponsePtr> JwtMiddleWare::invoke(const HttpRequestPtr& req, MiddlewareNextAwaiter&& next) {
    const std::string token = bearerToken(req);
    if (token.empty()) {
        co_return res::unauthorized("Authentication credentials were not provided.");
    }

    auto claims = co_await authenticate(services_, token);
    if (!claims) {
        co_return res::unauthorized("Given token not valid for any token type");
    }

    req->attributes()->insert("id", claims->userId);
    req->attributes()->insert("email", claims->email);
    req->attributes()->insert("device_id", claims->deviceId);
    co_return co_await next;
}

Task<HttpResponsePtr> OptionalJwtMiddleWare::invoke(const HttpRequestPtr& req, MiddlewareNextAwaiter&& next) {
    const std::string token = JwtMiddleWare::bearerToken(req);
    if (token.empty()) co_return co_await next;

    auto claims = co_await JwtMiddleWare::authenticate(services_, token);
    if (!claims) {
        co_return res::unauthorized("Given token not valid for any token type");
    }

    req->attributes()->insert("id", claims->userId);
    req->attributes()->insert("email", claims->email);
    req->attributes()->insert("device_id", claims->deviceId);
    co_return co_await next;
}
