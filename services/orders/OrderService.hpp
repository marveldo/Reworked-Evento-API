#pragma once
#include <drogon/orm/DbClient.h>
#include <drogon/utils/coroutine.h>
#include <json/json.h>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "input/QueryInputs.hpp"
#include "models/Orders.h"

using drogon_model::evento::Orders;

struct NewOrder {
    std::string userId;
    std::string eventId;
    std::string email;
    int64_t amount = 0;
    std::string currency = "NGN";
};

struct OrderPage {
    size_t count = 0;
    std::vector<Orders> items;
};

class OrderService {
    public:
        explicit OrderService(drogon::orm::DbClientPtr dbClient) : dbClient_(std::move(dbClient)) {}

        drogon::Task<Orders> create(const NewOrder& input, drogon::orm::DbClientPtr tx = nullptr);

        drogon::Task<Orders> getById(const std::string& id, drogon::orm::DbClientPtr tx = nullptr);

        drogon::Task<Orders> getByReference(const std::string& reference, drogon::orm::DbClientPtr tx = nullptr);

        drogon::Task<OrderPage> listForUser(const std::string& userId, const PageInput& page);

        drogon::Task<OrderPage> listForEvent(const std::string& eventId, const PageInput& page);

        drogon::Task<std::optional<Orders>> markPaid(const std::string& reference, drogon::orm::DbClientPtr tx = nullptr);

        drogon::Task<std::optional<Orders>> markFailed(const std::string& reference, const std::string& status,
                                                       drogon::orm::DbClientPtr tx = nullptr);

        drogon::Task<std::optional<Orders>> markRefunded(const std::string& reference, drogon::orm::DbClientPtr tx = nullptr);

        static std::string newReference();

        static Json::Value toJson(const Orders& order);

        drogon::Task<Json::Value> withEvents(const std::vector<Orders>& orders);

    private:
        drogon::orm::DbClientPtr client(const drogon::orm::DbClientPtr& tx) const { return tx ? tx : dbClient_; }

        drogon::Task<std::optional<Orders>> moveStatus(const std::string& reference, const std::string& from,
                                                       const std::string& to, drogon::orm::DbClientPtr tx);

        drogon::Task<OrderPage> list(const std::string& column, const std::string& value, const PageInput& page);

        drogon::orm::DbClientPtr dbClient_;
};
