#pragma once
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>

namespace jwt_time {
    // "15m", "30d" -> seconds
    inline std::chrono::seconds parse(const std::string& str) {
        if (str.size() < 2) throw std::invalid_argument("Invalid expiration '" + str + "' (use e.g. 15m, 12h, 30d)");
        const char unit = str.back();
        const int value = std::stoi(str.substr(0, str.size() - 1));
        switch (unit) {
            case 's': return std::chrono::seconds(value);
            case 'm': return std::chrono::minutes(value);
            case 'h': return std::chrono::hours(value);
            case 'd': return std::chrono::hours(value * 24);
            default: throw std::invalid_argument("Invalid time unit in expiration '" + str + "' (use s, m, h or d)");
        }
    }
}

struct Claims {
    std::string userId;
    std::string tokenType;   // "access" | "refresh"
    std::string email;
    std::string deviceId;    // the device_info row (login session) the token belongs to
};

struct Token {
    std::string token;
    std::string expiry;
};

// Access and refresh tokens are signed with different secrets, so one can never be used as the other.
// The user_id claim has the same name as in the Django (SimpleJWT) tokens.
class JwtService {
    public:
        JwtService(std::string accessSecret, std::string refreshSecret, const std::string& accessExpiration,
                   const std::string& refreshExpiration)
            : accessSecret_(std::move(accessSecret)), refreshSecret_(std::move(refreshSecret)),
              accessLifetime_(jwt_time::parse(accessExpiration)), refreshLifetime_(jwt_time::parse(refreshExpiration)) {}

        Token generateAccessToken(const Claims& claims);
        Token generateRefreshToken(const Claims& claims);

        // Throw if the signature, expiry or token type is wrong.
        std::shared_ptr<Claims> verifyAccessToken(const std::string& token);
        std::shared_ptr<Claims> verifyRefreshToken(const std::string& token);

    private:
        Token sign(const Claims& claims, const std::string& type, const std::string& secret, std::chrono::seconds lifetime);
        std::shared_ptr<Claims> verify(const std::string& token, const std::string& type, const std::string& secret);

        std::string accessSecret_;
        std::string refreshSecret_;
        std::chrono::seconds accessLifetime_;
        std::chrono::seconds refreshLifetime_;
};
