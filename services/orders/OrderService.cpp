#include "OrderService.hpp"
#include <drogon/orm/CoroMapper.h>
#include <drogon/orm/Exception.h>
#include <drogon/utils/Utilities.h>
#include <stdexcept>
#include "utils/Errors.hpp"
#include "models/Events.h"
#include <unordered_map>
#include <unordered_set>

using drogon::Task;
using namespace drogon::orm;
using drogon_model::evento::Events;

namespace {
    Json::Value timestamp(const std::shared_ptr<trantor::Date>& date) {
        return date ? Json::Value(date->toCustomFormattedStringLocal("%Y-%m-%dT%H:%M:%SZ")) : Json::Value();
    }

    Json::Value text(const std::shared_ptr<std::string>& value) {
        return value ? Json::Value(*value) : Json::Value();
    }
}

Json::Value OrderService::toJson(const Orders& order) {
    Json::Value item;
    item["id"]         = order.getValueOfId();
    item["reference"]  = order.getValueOfReference();
    item["status"]     = order.getValueOfStatus();
    item["amount"]     = static_cast<Json::Int64>(order.getValueOfAmount());
    item["currency"]   = order.getValueOfCurrency();
    item["email"]      = order.getValueOfEmail();
    item["event_id"]   = text(order.getEventId());
    item["paid_at"]    = timestamp(order.getPaidAt());
    item["created_at"] = timestamp(order.getCreatedAt());
    return item;
}

Task<Json::Value> OrderService::withEvents(const std::vector<Orders>& orders) {
    std::unordered_set<std::string> seen;
    std::vector<std::string> eventIds;
    for (const auto& order : orders) {
        if (order.getEventId() && seen.insert(order.getValueOfEventId()).second) eventIds.push_back(order.getValueOfEventId());
    }

    std::unordered_map<std::string, Json::Value> events;
    if (!eventIds.empty()) {
        const auto rows = co_await CoroMapper<Events>(dbClient_).findBy(Criteria(Events::Cols::_id, CompareOperator::In, eventIds));
        for (const auto& event : rows) {
            Json::Value item;
            item["id"]          = event.getValueOfId();
            item["event_name"]  = text(event.getEventName());
            item["event_image"] = text(event.getEventImage());
            item["start_date"]  = event.getStartDate() ? Json::Value(event.getStartDate()->toCustomFormattedStringLocal("%Y-%m-%d"))
                                                       : Json::Value();
            events.emplace(event.getValueOfId(), item);
        }
    }

    Json::Value list(Json::arrayValue);
    for (const auto& order : orders) {
        auto item = toJson(order);
        const auto it = order.getEventId() ? events.find(order.getValueOfEventId()) : events.end();
        item["event"] = it == events.end() ? Json::Value() : it->second;
        list.append(item);
    }
    co_return list;
}

std::string OrderService::newReference() {
    return "evt_" + drogon::utils::getUuid();
}

Task<Orders> OrderService::create(const NewOrder& input, DbClientPtr tx) {
    if (input.amount <= 0) throw std::invalid_argument("order amount must be positive");

    Orders order;
    order.setUserId(input.userId);
    order.setEventId(input.eventId);
    order.setEmail(input.email);
    order.setAmount(input.amount);
    order.setCurrency(input.currency);
    order.setReference(newReference());
    co_return co_await CoroMapper<Orders>(client(tx)).insert(order);
}

Task<Orders> OrderService::getById(const std::string& id, DbClientPtr tx) {
    const auto orders = co_await CoroMapper<Orders>(client(tx)).findBy(Criteria(Orders::Cols::_id, id));
    if (orders.empty()) throw NotFound("Order not found");
    co_return orders[0];
}

Task<Orders> OrderService::getByReference(const std::string& reference, DbClientPtr tx) {
    const auto orders = co_await CoroMapper<Orders>(client(tx)).findBy(Criteria(Orders::Cols::_reference, reference));
    if (orders.empty()) throw NotFound("Order not found");
    co_return orders[0];
}

Task<OrderPage> OrderService::list(const std::string& column, const std::string& value, const PageInput& page) {
    OrderPage result;
    const Criteria criteria(column, value);
    result.count = co_await CoroMapper<Orders>(dbClient_).count(criteria);
    result.items = co_await CoroMapper<Orders>(dbClient_)
                       .orderBy(Orders::Cols::_created_at, SortOrder::DESC)
                       .orderBy(Orders::Cols::_id, SortOrder::DESC)
                       .limit(page.limit)
                       .offset(page.offset)
                       .findBy(criteria);
    co_return result;
}

Task<OrderPage> OrderService::listForUser(const std::string& userId, const PageInput& page) {
    co_return co_await list(Orders::Cols::_user_id, userId, page);
}

Task<OrderPage> OrderService::listForEvent(const std::string& eventId, const PageInput& page) {
    co_return co_await list(Orders::Cols::_event_id, eventId, page);
}

Task<std::optional<Orders>> OrderService::markPaid(const std::string& reference, DbClientPtr tx) {
    const auto rows = co_await client(tx)->execSqlCoro(
        "UPDATE orders SET status = 'success', paid_at = NOW() "
        "WHERE reference = $1 AND status = 'pending' RETURNING *",
        reference);
    if (rows.empty()) co_return std::nullopt;
    co_return Orders(rows[0]);
}

Task<std::optional<Orders>> OrderService::markFailed(const std::string& reference, const std::string& status, DbClientPtr tx) {
    if (status != "failed" && status != "abandoned") throw std::invalid_argument("status must be failed or abandoned");
    co_return co_await moveStatus(reference, "pending", status, tx);
}

Task<std::optional<Orders>> OrderService::markRefunded(const std::string& reference, DbClientPtr tx) {
    co_return co_await moveStatus(reference, "success", "refunded", tx);
}

Task<std::optional<Orders>> OrderService::moveStatus(const std::string& reference, const std::string& from,
                                                     const std::string& to, DbClientPtr tx) {
    const auto rows = co_await client(tx)->execSqlCoro(
        "UPDATE orders SET status = $1 WHERE reference = $2 AND status = $3 RETURNING *", to, reference, from);
    if (rows.empty()) co_return std::nullopt;
    co_return Orders(rows[0]);
}
