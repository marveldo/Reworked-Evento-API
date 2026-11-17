#include "GoogleAuthService.hpp"
#include <trantor/utils/Logger.h>
#include <json/json.h>

GoogleAuthService::GoogleAuthService()
    : clients_("https://oauth2.googleapis.com") {}

drogon::Task<std::optional<GoogleIdentity>> GoogleAuthService::verifyIdToken(std::string idToken) {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/tokeninfo");
    req->setParameter("id_token", idToken);

    auto resp = co_await clients_.get()->sendRequestCoro(req, 10);
    if (resp->statusCode() != drogon::k200OK) co_return std::nullopt;

    const auto json = resp->getJsonObject();
    if (!json) co_return std::nullopt;
    const auto field = [&](const char* name) { return (*json).get(name, "").asString(); };

    const std::string issuer = field("iss");
    if (issuer != "accounts.google.com" && issuer != "https://accounts.google.com") co_return std::nullopt;

    // Accounts are matched by email (UserService::findOrCreateSocialUser), so an unverified Google
    // email would let anyone sign in as the existing user with that address. tokeninfo sends "true"
    // as a string; accept a JSON bool too.
    const Json::Value verified = (*json).get("email_verified", false);
    if (!(verified.isBool() ? verified.asBool() : verified.asString() == "true")) co_return std::nullopt;

    GoogleIdentity identity;
    identity.subject    = field("sub");
    identity.audience   = field("aud");
    identity.email      = field("email");
    identity.name       = field("name");
    identity.givenName  = field("given_name");
    identity.familyName = field("family_name");
    identity.picture    = field("picture");
    if (identity.subject.empty() || identity.email.empty()) co_return std::nullopt;
    co_return identity;
}
