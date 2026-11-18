#pragma once
#include <drogon/HttpClient.h>
#include "utils/HttpClientPool.hpp"
#include <drogon/utils/coroutine.h>
#include <inja/inja.hpp>
#include <nlohmann/json.hpp>
#include <string>

class EmailService {
public:
    EmailService(std::string apiKey, std::string from, const std::string& templatesDir, std::string appUrl, std::string assetsUrl, trantor::EventLoop* loop, int connections);

    drogon::Task<> send(std::string to, std::string subject, std::string templateName, nlohmann::json data);

private:
    std::string apiKey_;
    std::string from_;
    std::string appUrl_;
    std::string assetsUrl_;
    trantor::EventLoop* loop_;
    inja::Environment env_;
    HttpClientPool clients_;
};
