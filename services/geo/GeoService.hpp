#pragma once
#include "utils/HttpClientPool.hpp"
#include <drogon/utils/coroutine.h>
#include <optional>
#include <string>

class GeoService {
public:
    explicit GeoService(std::string ipinfoToken);

    drogon::Task<std::optional<std::string>> locate(std::string ip);

    static bool isPublicIp(const std::string& ip);

private:
    std::string token_;
    HttpClientPool clients_;
};
