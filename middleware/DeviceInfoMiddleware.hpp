#pragma once
#include <drogon/HttpMiddleware.h>
#include <string>

class DeviceInfoMiddleware : public drogon::HttpMiddleware<DeviceInfoMiddleware>
{
public:
    void invoke(const drogon::HttpRequestPtr& req,
                drogon::MiddlewareNextCallback&& nextCb,
                drogon::MiddlewareCallback&& mcb) override;

    static std::string clientIp(const drogon::HttpRequestPtr& req);
};
