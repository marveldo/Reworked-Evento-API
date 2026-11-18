#pragma once
#include "utils/HttpClientPool.hpp"
#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>
#include <nlohmann/json.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include "events/EventBus.hpp"
#include "services/events/EventService.hpp"
#include "services/orders/OrderService.hpp"

struct CheckoutInput {
    std::string email;
    int64_t amount = 0;
    std::string currency = "NGN";
    std::string reference;
    std::optional<std::string> subaccount;
};

struct Checkout {
    std::string authorizationUrl;
    std::string accessCode;
};

struct PaystackStatus {
    std::string status;
    int64_t amount = 0;
    std::string currency;
};

class PaymentsNotConfigured : public std::runtime_error {
    public:
        PaymentsNotConfigured() : std::runtime_error("PAYSTACK_SECRET_KEY is not set") {}
};

class PaymentService {
    public :
        PaymentService(std::string apiKey, std::string url, std::string callbackUrl, drogon::orm::DbClientPtr dbClient,
                       OrderService* orders, EventService* events, EventBus* bus);

        bool configured() const { return !apiKey_.empty(); }

        drogon::Task<Checkout> initialize(const CheckoutInput& input);

        drogon::Task<PaystackStatus> verify(const std::string& reference);

        bool validSignature(std::string_view body, std::string_view signature) const;

        drogon::Task<std::string> settle(const std::string& reference);

        void queueSettle(const std::string& reference);

    private:
        drogon::HttpRequestPtr request(drogon::HttpMethod method, const std::string& path) const;
        static nlohmann::json dataOf(const drogon::HttpResponsePtr& resp);

        std::string apiKey_;
        std::string callbackUrl_;
        HttpClientPool clients_;
        drogon::orm::DbClientPtr dbClient_;
        OrderService* orders_;
        EventService* events_;
        EventBus* bus_;
};
