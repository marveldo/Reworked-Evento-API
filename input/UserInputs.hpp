#pragma once
#include "utils/Validation.hpp"
#include "utils/RequestBody.hpp"
#include "utils/Media.hpp"
#include <algorithm>
#include <cctype>
#include <optional>
#include <regex>
#include <string>

namespace user_rules {
    inline const std::regex email(R"([^@\s]+@[^@\s]+\.[^@\s]+)");

    inline std::optional<std::string> lower(std::optional<std::string> value) {
        if (value) std::transform(value->begin(), value->end(), value->begin(), [](unsigned char c) { return std::tolower(c); });
        return value;
    }

    inline std::optional<std::string> trimmed(std::optional<std::string> value) {
        if (!value) return value;
        const auto start = value->find_first_not_of(" \t\r\n");
        const auto end   = value->find_last_not_of(" \t\r\n");
        return start == std::string::npos ? std::string() : value->substr(start, end - start + 1);
    }
}

struct ProfileFields {
    std::optional<std::string> location;
    std::optional<std::string> website;
    std::optional<std::string> facebook;
    std::optional<std::string> twitter;
    std::optional<std::string> instagram;
    std::optional<std::string> bio;

    bool any() const { return location || website || facebook || twitter || instagram || bio; }
};

inline ProfileFields readProfileFields(Validator& v) {
    return ProfileFields{
        v.optionalString("location", 0, 500),
        v.optionalString("website", 0, 200),
        v.optionalString("facebook", 0, 200),
        v.optionalString("twitter", 0, 200),
        v.optionalString("instagram", 0, 200),
        v.optionalString("bio", 0, 10000),
    };
}

struct RegisterInput {
    std::string fullName;
    std::string email;
    std::string password;               // plain text from the client: hash it before saving
    ProfileFields profile;
};

inline ParseResult<RegisterInput> parseRegister(const nlohmann::json& body) {
    return parseFields<RegisterInput>(body, [](Validator& v) {
        auto fullName = user_rules::trimmed(v.requiredString("full_name", 1, 255));
        auto email    = user_rules::lower(user_rules::trimmed(v.requiredString("email", 3, 254)));
        auto password = v.requiredString("password", 8, 128);
        if (fullName && fullName->empty()) v.addError("full_name", "Field Cannot be empty");
        v.matches(email, "email", user_rules::email, "Enter a valid email address.");
        auto profile = readProfileFields(v);
        return RegisterInput{fullName.value_or(""), email.value_or(""), password.value_or(""), profile};
    });
}

struct LoginInput {
    std::string email;
    std::string password;
};

inline ParseResult<LoginInput> parseLogin(const nlohmann::json& body) {
    return parseFields<LoginInput>(body, [](Validator& v) {
        auto email    = user_rules::lower(user_rules::trimmed(v.requiredString("email", 1, 254)));
        auto password = v.requiredString("password", 1, 128);
        return LoginInput{email.value_or(""), password.value_or("")};
    });
}

struct RefreshInput {
    std::string refresh;
    std::optional<std::string> deviceId;
};

inline ParseResult<RefreshInput> parseRefresh(const nlohmann::json& body) {
    return parseFields<RefreshInput>(body, [](Validator& v) {
        auto refresh  = v.requiredString("refresh", 1, 4096);
        auto deviceId = v.optionalString("device_id", 0, 64);
        return RefreshInput{refresh.value_or(""), deviceId};
    });
}

struct GoogleSigninInput {
    std::string idToken;
};

inline ParseResult<GoogleSigninInput> parseGoogleSignin(const nlohmann::json& body) {
    return parseFields<GoogleSigninInput>(body, [](Validator& v) {
        auto token = v.requiredString("access_token", 6, 8192);
        return GoogleSigninInput{token.value_or("")};
    });
}

struct LogoutInput {
    std::string deviceId;
    std::string refreshToken;
};

inline ParseResult<LogoutInput> parseLogout(const nlohmann::json& body) {
    return parseFields<LogoutInput>(body, [](Validator& v) {
        auto deviceId = v.requiredString("device_id", 1, 64);
        auto refresh  = v.requiredString("refresh_token", 1, 4096);
        return LogoutInput{deviceId.value_or(""), refresh.value_or("")};
    });
}

struct UpdateUserInput {
    std::optional<std::string> fullName;
    std::optional<std::string> email;
    std::optional<std::string> password;
    std::optional<std::string> profileImg;
    ProfileFields profile;

    bool any() const { return fullName || email || password || profileImg || profile.any(); }
};

inline ParseResult<UpdateUserInput> parseUpdateUser(const RequestBody& body) {
    nlohmann::json fields = body.fields;
    const bool pictureFile = body.hasFile("profile_pic");
    fields.erase("profile_pic");

    return parseFields<UpdateUserInput>(fields, [pictureFile](Validator& v) {
        if (pictureFile) {
            v.addError("profile_pic", "Upload the picture to Cloudinary with a signature from POST /api/v1/photos/signature/ (kind=profile) and send its public_id as profile_img");
        }
        UpdateUserInput in;
        in.fullName   = user_rules::trimmed(v.optionalString("full_name", 1, 255));
        in.email      = user_rules::lower(user_rules::trimmed(v.optionalString("email", 3, 254)));
        in.password   = v.optionalString("password", 8, 128);
        in.profileImg = v.optionalString("profile_img", 1, 255);
        in.profile    = readProfileFields(v);
        if (in.fullName && in.fullName->empty()) v.addError("full_name", "Field Cannot be empty");
        v.matches(in.email, "email", user_rules::email, "Enter a valid email address.");
        if (in.profileImg && !media::isPhotoId(*in.profileImg, media::PhotoKind::Profile)) {
            v.addError("profile_img", "must be the public_id from POST /api/v1/photos/signature/ with kind=profile");
        }
        return in;
    });
}
