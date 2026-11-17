#pragma once
#include <drogon/HttpMiddleware.h>
#include "services/Services.hpp"

class JwtMiddleWare : public drogon::HttpCoroMiddleware<JwtMiddleWare, false>
{
public:
    explicit JwtMiddleWare(Services* services) : services_(services) {}

    drogon::Task<drogon::HttpResponsePtr> invoke(const drogon::HttpRequestPtr& req,
                                                 drogon::MiddlewareNextAwaiter&& next) override;

    static std::string bearerToken(const drogon::HttpRequestPtr& req);

    static drogon::Task<std::shared_ptr<Claims>> authenticate(Services* services, std::string token);

private:
    Services* services_;
};
