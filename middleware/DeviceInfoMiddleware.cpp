#include "DeviceInfoMiddleware.hpp"
#include "utils/UserAgent.hpp"

using namespace drogon;

std::string DeviceInfoMiddleware::clientIp(const HttpRequestPtr& req) {
    const std::string& forwarded = req->getHeader("X-Forwarded-For");
    if (!forwarded.empty()) {
        std::string first = forwarded.substr(0, forwarded.find(','));
        const auto start = first.find_first_not_of(" \t");
        const auto end   = first.find_last_not_of(" \t");
        if (start != std::string::npos) return first.substr(start, end - start + 1);
    }
    return req->getPeerAddr().toIp();
}

void DeviceInfoMiddleware::invoke(const HttpRequestPtr& req, MiddlewareNextCallback&& nextCb, MiddlewareCallback&& mcb) {
    const DeviceDetails device = user_agent::parse(req->getHeader("User-Agent"), req->getHeader("Sec-CH-UA-Model"));

    req->attributes()->insert("ip_address", clientIp(req));
    req->attributes()->insert("device_type", device.type);
    req->attributes()->insert("device_name", device.name);
    req->attributes()->insert("device_os", device.os);

    nextCb([mcb = std::move(mcb)](const HttpResponsePtr& resp) { mcb(resp); });
}
