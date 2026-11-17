#pragma once
#include "utils/Validation.hpp"
#include "utils/RequestBody.hpp"
#include "utils/Media.hpp"
#include <cstdint>
#include <cstdio>
#include <optional>
#include <regex>
#include <string>

namespace event_rules {
    inline bool validDate(const std::string& value) {
        static const std::regex shape(R"(\d{4}-\d{2}-\d{2})");
        if (!std::regex_match(value, shape)) return false;
        const int year = std::stoi(value.substr(0, 4)), month = std::stoi(value.substr(5, 2)), day = std::stoi(value.substr(8, 2));
        if (year < 1 || month < 1 || month > 12 || day < 1) return false;
        static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        const bool leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
        return day <= days[month - 1] + (month == 2 && leap ? 1 : 0);
    }

    inline std::optional<std::string> normalTime(const std::string& value) {
        static const std::regex shape(R"(([01]\d|2[0-3]):([0-5]\d)(?::([0-5]\d)(\.\d{1,6})?)?)");
        std::smatch m;
        if (!std::regex_match(value, m, shape)) return std::nullopt;
        return m[1].str() + ":" + m[2].str() + ":" + (m[3].matched ? m[3].str() : "00") + (m[4].matched ? m[4].str() : "");
    }

    inline std::optional<std::string> date(Validator& v, const std::string& field) {
        auto value = v.optionalString(field, 0, 10);
        if (value && !validDate(*value)) {
            v.addError(field, "Date has wrong format. Use one of these formats instead: YYYY-MM-DD.");
            return std::nullopt;
        }
        return value;
    }

    inline std::optional<std::string> time(Validator& v, const std::string& field) {
        auto value = v.optionalString(field, 0, 15);
        if (!value) return value;
        auto normal = normalTime(*value);
        if (!normal) v.addError(field, "Time has wrong format. Use one of these formats instead: hh:mm[:ss[.uuuuuu]].");
        return normal;
    }
}

struct CreateEventInput {
    std::optional<std::string> eventName;
    std::optional<std::string> date;
    std::optional<std::string> description;
    std::optional<std::string> startDate;
    std::optional<std::string> startTime;
    std::optional<std::string> time;
    std::optional<int64_t>     price;
    std::optional<std::string> location;
    std::optional<std::string> eventImage;
    std::optional<int64_t>     capacity;
    std::string                eventCategory;
};

inline ParseResult<CreateEventInput> parseCreateEvent(const RequestBody& body) {
    nlohmann::json fields = body.fields;
    for (const char* number : {"price", "capacity"}) body_fields::numberFromText(fields, number);
    for (const char* optional : {"date", "start_date", "time", "start_time", "event_image"}) body_fields::emptyToNull(fields, optional);
    const bool imageFile = body.hasFile("event_image");

    return parseFields<CreateEventInput>(fields, [imageFile](Validator& v) {
        CreateEventInput in;
        in.eventName   = v.optionalString("event_name", 0, 250);
        in.date        = event_rules::date(v, "date");
        in.description = v.optionalString("description", 0, 20000);
        in.startDate   = event_rules::date(v, "start_date");
        in.startTime   = event_rules::time(v, "start_time");
        in.time        = event_rules::time(v, "time");
        in.price       = v.optionalInt("price", 0, 2147483647);
        in.location    = v.optionalString("location", 0, 400);
        in.capacity    = v.optionalInt("capacity", 0, 2147483647);
        in.eventCategory = v.requiredString("event_category", 1, 200).value_or("");

        if (imageFile) {
            v.addError("event_image", "Upload the image to Cloudinary with a signature from POST /api/v1/photos/signature/ (kind=event) and send its public_id as event_image");
        } else {
            in.eventImage = v.optionalString("event_image", 1, 255);
            if (in.eventImage && !media::isPhotoId(*in.eventImage, media::PhotoKind::Event)) {
                v.addError("event_image", "must be the public_id from POST /api/v1/photos/signature/ with kind=event");
            }
        }
        return in;
    });
}

struct RegisterAttendeeInput {
    std::string email;
};

inline ParseResult<RegisterAttendeeInput> parseRegisterAttendee(const nlohmann::json& body) {
    return parseFields<RegisterAttendeeInput>(body, [](Validator& v) {
        auto email = v.requiredString("email", 1, 254);
        if (email) {
            for (auto& c : *email) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return RegisterAttendeeInput{email.value_or("")};
    });
}
