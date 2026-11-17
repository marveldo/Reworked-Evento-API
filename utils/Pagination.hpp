#pragma once
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <drogon/utils/Utilities.h>
#include <json/json.h>
#include <map>
#include <string>
#include "controllers/Api.hpp"
#include "input/QueryInputs.hpp"

namespace pagination {

inline std::string pageUrl(const drogon::HttpRequestPtr& req, const std::map<std::string, std::string>& changes) {
    std::string scheme = req->getHeader("X-Forwarded-Proto");
    if (scheme.empty()) scheme = req->isOnSecureConnection() ? "https" : "http";
    std::string host = req->getHeader("X-Forwarded-Host");
    if (host.empty()) host = req->getHeader("Host");

    std::map<std::string, std::string> params;
    for (const auto& [key, value] : req->getParameters()) params[key] = value;
    for (const auto& [key, value] : changes) {
        if (value.empty()) params.erase(key);
        else params[key] = value;
    }

    std::string query;
    for (const auto& [key, value] : params) {
        query += (query.empty() ? "?" : "&") + drogon::utils::urlEncodeComponent(key) + "=" + drogon::utils::urlEncodeComponent(value);
    }
    return scheme + "://" + host + req->path() + query;
}

inline drogon::HttpResponsePtr response(const drogon::HttpRequestPtr& req, const PageInput& page, size_t count,
                                        const Json::Value& data, const std::string& message = "Querysuccessful") {
    Json::Value body;
    body["status"] = 200;

    const std::string limit = std::to_string(page.limit);
    if (page.offset + page.limit >= count) {
        body["links"]["next_link"] = Json::nullValue;
    } else {
        body["links"]["next_link"] = pageUrl(req, {{"limit", limit}, {"offset", std::to_string(page.offset + page.limit)}});
    }
    if (page.offset == 0) {
        body["links"]["previous_link"] = Json::nullValue;
    } else if (page.offset <= page.limit) {
        body["links"]["previous_link"] = pageUrl(req, {{"limit", limit}, {"offset", ""}});
    } else {
        body["links"]["previous_link"] = pageUrl(req, {{"limit", limit}, {"offset", std::to_string(page.offset - page.limit)}});
    }

    body["message"]     = message;
    body["limit"]       = static_cast<Json::UInt64>(page.limit);
    body["offset"]      = static_cast<Json::UInt64>(page.offset);
    body["count"]       = static_cast<Json::UInt64>(count);
    body["total_pages"] = static_cast<Json::UInt64>((count + page.limit - 1) / page.limit);
    body["data"]        = data;
    return res::json(drogon::k200OK, body);
}

}
