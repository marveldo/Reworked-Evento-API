#pragma once
#include <drogon/HttpMiddleware.h>

// Rate limits for single routes (the limits themselves are in utils/RateLimit.hpp).
// The per-user ones go after JwtMiddleWare, which sets the user id they key on.

// Login: per email being logged into, before any password hashing.
class LoginRateLimit : public drogon::HttpCoroMiddleware<LoginRateLimit> {
public:
    drogon::Task<drogon::HttpResponsePtr> invoke(const drogon::HttpRequestPtr& req, drogon::MiddlewareNextAwaiter&& next) override;
};

class SignupRateLimit : public drogon::HttpCoroMiddleware<SignupRateLimit> {
public:
    drogon::Task<drogon::HttpResponsePtr> invoke(const drogon::HttpRequestPtr& req, drogon::MiddlewareNextAwaiter&& next) override;
};

// Token refresh: per user id in the refresh token.
class RefreshRateLimit : public drogon::HttpCoroMiddleware<RefreshRateLimit> {
public:
    drogon::Task<drogon::HttpResponsePtr> invoke(const drogon::HttpRequestPtr& req, drogon::MiddlewareNextAwaiter&& next) override;
};

class SignatureRateLimit : public drogon::HttpCoroMiddleware<SignatureRateLimit> {
public:
    drogon::Task<drogon::HttpResponsePtr> invoke(const drogon::HttpRequestPtr& req, drogon::MiddlewareNextAwaiter&& next) override;
};
