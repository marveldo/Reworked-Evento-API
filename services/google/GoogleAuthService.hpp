#pragma once
#include "utils/HttpClientPool.hpp"
#include <drogon/utils/coroutine.h>
#include <optional>
#include <string>

struct GoogleIdentity {
    std::string subject;
    std::string audience;
    std::string email;
    std::string name;
    std::string givenName;
    std::string familyName;
    std::string picture;
};

class GoogleAuthService {
public:
    GoogleAuthService();

    drogon::Task<std::optional<GoogleIdentity>> verifyIdToken(std::string idToken);

private:
    HttpClientPool clients_;
};
