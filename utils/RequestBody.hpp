#pragma once
#include <drogon/HttpRequest.h>
#include <drogon/MultiPart.h>
#include <nlohmann/json.hpp>
#include <optional>
#include <regex>
#include <string>
#include <unordered_map>
#include "utils/Validation.hpp"

struct RequestBody {
    nlohmann::json fields = nlohmann::json::object();
    std::unordered_map<std::string, drogon::HttpFile> files;
    std::optional<FieldError> error;

    bool hasFile(const std::string& name) const { return files.count(name) > 0; }
};

inline RequestBody readBody(const drogon::HttpRequestPtr& req) {
    RequestBody body;
    const auto type = req->contentType();

    if (type == drogon::CT_MULTIPART_FORM_DATA) {
        drogon::MultiPartParser parser;
        if (parser.parse(req) != 0) {
            body.error = FieldError{"body", "is not valid multipart/form-data"};
            return body;
        }
        for (const auto& [key, value] : parser.getParameters()) body.fields[key] = value;
        for (const auto& file : parser.getFiles()) body.files.emplace(file.getItemName(), file);
        return body;
    }

    if (type == drogon::CT_APPLICATION_X_FORM) {
        for (const auto& [key, value] : req->getParameters()) body.fields[key] = value;
        return body;
    }

    if (req->body().empty()) return body;
    auto json = nlohmann::json::parse(req->body(), nullptr, false);
    if (json.is_discarded()) {
        body.error = FieldError{"body", "is not valid JSON"};
    } else if (!json.is_object()) {
        body.error = FieldError{"body", "must be a JSON object"};
    } else {
        body.fields = std::move(json);
    }
    return body;
}

namespace body_fields {
    inline void numberFromText(nlohmann::json& fields, const std::string& name) {
        if (!fields.contains(name) || !fields[name].is_string()) return;
        const std::string text = fields[name].get<std::string>();
        static const std::regex integer(R"(\s*-?\d{1,18}\s*)");
        if (text.empty()) fields[name] = nullptr;
        else if (std::regex_match(text, integer)) fields[name] = std::stoll(text);
    }

    inline void emptyToNull(nlohmann::json& fields, const std::string& name) {
        if (fields.contains(name) && fields[name].is_string() && fields[name].get<std::string>().empty()) {
            fields[name] = nullptr;
        }
    }
}
