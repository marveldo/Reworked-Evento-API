#pragma once
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include <optional>
#include <string>
#include <vector>
#include "utils/Validation.hpp"

inline const std::string API_V1 = "/api/v1";

// Responses keep the shapes the Django API sent (api/utils.py), so Evento-frontend keeps working:
//
//   success:     {"status": 200, "message": "...", "access": "...", "refresh": "...", "data": ...}
//   validation:  {"errors": [{"field": "email", "message": "..."}]}          (422)
//   error:       {"status": 401, "message": "...", "detail": "..."}
//   paginated:   {"status", "links": {"next_link", "previous_link"}, "message", "limit", "offset",
//                 "count", "total_pages", "data"}                            (see utils/Pagination.hpp)
namespace res {

inline drogon::HttpResponsePtr json(drogon::HttpStatusCode status, const Json::Value& body) {
    auto r = drogon::HttpResponse::newHttpJsonResponse(body);
    r->setStatusCode(status);
    return r;
}

struct Tokens {
    std::string access;
    std::string refresh;
    std::string access_expiry ;
    std::string refresh_expiry;
};

// success_response(status_code, message, access_token, refresh_token, data)
inline drogon::HttpResponsePtr success(drogon::HttpStatusCode status, const std::string& message,
                                       const Json::Value& data = Json::nullValue,
                                       const std::optional<Tokens>& tokens = std::nullopt) {
    Json::Value body;
    body["status"]  = static_cast<int>(status);
    body["message"] = message;
    if (tokens) {
        body["access"]  = tokens->access;
        body["refresh"] = tokens->refresh;
        if (!tokens->access_expiry.empty())  body["access_expiry"]  = tokens->access_expiry;
        if (!tokens->refresh_expiry.empty()) body["refresh_expiry"] = tokens->refresh_expiry;
    }
    if (!data.isNull()) body["data"] = data;
    return json(status, body);
}

inline drogon::HttpResponsePtr error(drogon::HttpStatusCode status, const std::string& message) {
    Json::Value body;
    body["status"]  = static_cast<int>(status);
    body["message"] = message;
    body["detail"]  = message;   // DRF put errors in "detail"; the frontend reads "message"
    return json(status, body);
}

// error_validation(serializer, 422)
inline drogon::HttpResponsePtr validation(const std::vector<FieldError>& errors,
                                          drogon::HttpStatusCode status = drogon::k422UnprocessableEntity) {
    Json::Value list(Json::arrayValue);
    for (const auto& e : errors) {
        Json::Value item;
        item["field"]   = e.field;
        item["message"] = e.message;
        list.append(item);
    }
    Json::Value body;
    body["errors"] = list;
    return json(status, body);
}

inline drogon::HttpResponsePtr validation(const std::string& field, const std::string& message,
                                          drogon::HttpStatusCode status = drogon::k422UnprocessableEntity) {
    return validation(std::vector<FieldError>{{field, message}}, status);
}

inline drogon::HttpResponsePtr unauthorized(const std::string& message = "Authentication credentials were not provided.") {
    return error(drogon::k401Unauthorized, message);
}

inline drogon::HttpResponsePtr notFound(const std::string& message = "Not found.") {
    return error(drogon::k404NotFound, message);
}

}
