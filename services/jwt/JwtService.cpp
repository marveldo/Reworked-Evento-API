#include "JwtService.hpp"
#include <drogon/utils/Utilities.h>
#include <jwt-cpp/traits/nlohmann-json/defaults.h>
#include <format>

Token JwtService::sign(const Claims& claims, const std::string& type, const std::string& secret,
                             std::chrono::seconds lifetime) {
    Token token;
    // Whole seconds, so the reported expiry is exactly the token's exp claim.
    const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
    const auto expiryTime = now + lifetime;
    token.token = jwt::create()
        .set_type("JWT")
        .set_subject(claims.userId)
        .set_id(drogon::utils::getUuid())
        .set_issued_at(now)
        .set_expires_at(expiryTime)
        .set_payload_claim("user_id", jwt::claim(claims.userId))
        .set_payload_claim("token_type", jwt::claim(type))
        .set_payload_claim("email", jwt::claim(claims.email))
        .set_payload_claim("device_id", jwt::claim(claims.deviceId))
        .sign(jwt::algorithm::hs256{secret});
    token.expiry = std::format("{:%Y-%m-%dT%H:%M:%SZ}", expiryTime);   // ISO 8601, UTC
    return token;
}

std::shared_ptr<Claims> JwtService::verify(const std::string& token, const std::string& type, const std::string& secret) {
    const auto decoded = jwt::decode(token);
    jwt::verify().allow_algorithm(jwt::algorithm::hs256{secret}).verify(decoded);

    auto json = decoded.get_payload_json();
    auto claims = std::make_shared<Claims>();
    claims->userId    = json.at("user_id").get<std::string>();
    claims->tokenType = json.at("token_type").get<std::string>();
    claims->email     = json.at("email").get<std::string>();
    claims->deviceId  = json.at("device_id").get<std::string>();
    if (claims->tokenType != type) throw std::runtime_error("wrong token type");
    return claims;
}

Token JwtService::generateAccessToken(const Claims& claims) {
    return sign(claims, "access", accessSecret_, accessLifetime_);
}

Token JwtService::generateRefreshToken(const Claims& claims) {
    return sign(claims, "refresh", refreshSecret_, refreshLifetime_);
}

std::shared_ptr<Claims> JwtService::verifyAccessToken(const std::string& token) {
    return verify(token, "access", accessSecret_);
}

std::shared_ptr<Claims> JwtService::verifyRefreshToken(const std::string& token) {
    return verify(token, "refresh", refreshSecret_);
}
