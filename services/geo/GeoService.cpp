#include "GeoService.hpp"
#include <drogon/utils/Utilities.h>
#include <json/json.h>
#include <regex>
#include <stdexcept>

GeoService::GeoService(std::string ipinfoToken)
    : token_(std::move(ipinfoToken)), clients_("https://ipinfo.io") {}

bool GeoService::isPublicIp(const std::string& ip) {
    static const std::regex ipv4(R"((\d{1,3})\.(\d{1,3})\.\d{1,3}\.\d{1,3})");
    std::smatch m;
    if (std::regex_match(ip, m, ipv4)) {
        const int a = std::stoi(m[1]), b = std::stoi(m[2]);
        if (a == 10 || a == 127 || a == 0) return false;
        if (a == 192 && b == 168) return false;
        if (a == 172 && b >= 16 && b <= 31) return false;
        if (a == 169 && b == 254) return false;
        if (a == 100 && b >= 64 && b <= 127) return false;
        return true;
    }
    static const std::regex ipv6(R"([0-9a-fA-F:]+)");
    if (!std::regex_match(ip, ipv6) || ip.find(':') == std::string::npos) return false;
    return !(ip == "::1" || ip.rfind("fe80", 0) == 0 || ip.rfind("fc", 0) == 0 || ip.rfind("fd", 0) == 0);
}

drogon::Task<std::optional<std::string>> GeoService::locate(std::string ip) {
    if (!isPublicIp(ip)) co_return std::nullopt;

    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Get);
    req->setPath("/" + ip + "/json");
    if (!token_.empty()) req->setParameter("token", token_);

    auto resp = co_await clients_.get()->sendRequestCoro(req, 10);
    if (resp->statusCode() == drogon::k429TooManyRequests) throw std::runtime_error("ipinfo rate limit reached");
    if (resp->statusCode() != drogon::k200OK) co_return std::nullopt;

    const auto json = resp->getJsonObject();
    if (!json || (*json).get("bogon", false).asBool()) co_return std::nullopt;
    const std::string country = (*json).get("country", "").asString();
    const std::string city    = (*json).get("city", "").asString();
    if (country.empty() || city.empty()) co_return std::nullopt;
    co_return country + "," + city;
}
