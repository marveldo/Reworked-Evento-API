#include "PaymentService.hpp"
#include <drogon/orm/Exception.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <trantor/utils/Logger.h>
#include <cstdio>
#include <stdexcept>
#include "events/Events.hpp"
#include "utils/DbErrors.hpp"
#include "utils/DbTransaction.hpp"

using drogon::Task;
using drogon::orm::DbClientPtr;

namespace {
    std::string toHex(const unsigned char* bytes, unsigned int length) {
        std::string hex;
        hex.reserve(length * 2);
        char byte[3];
        for (unsigned int i = 0; i < length; i++) {
            std::snprintf(byte, sizeof(byte), "%02x", bytes[i]);
            hex += byte;
        }
        return hex;
    }
}

PaymentService::PaymentService(std::string apiKey, std::string url, std::string callbackUrl, DbClientPtr dbClient,
                               OrderService* orders, EventService* events, EventBus* bus)
    : apiKey_(std::move(apiKey)),
      callbackUrl_(std::move(callbackUrl)),
      clients_(std::move(url)),
      dbClient_(std::move(dbClient)),
      orders_(orders),
      events_(events),
      bus_(bus) {}

drogon::HttpRequestPtr PaymentService::request(drogon::HttpMethod method, const std::string& path) const {
    if (!configured()) throw PaymentsNotConfigured();
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(method);
    req->setPath(path);
    req->addHeader("Authorization", "Bearer " + apiKey_);
    return req;
}

nlohmann::json PaymentService::dataOf(const drogon::HttpResponsePtr& resp) {
    const auto body = nlohmann::json::parse(resp->body(), nullptr, false);
    if (resp->statusCode() != drogon::k200OK || !body.is_object() || !body.value("status", false) || !body.contains("data")) {
        const std::string message = body.is_object() ? body.value("message", "") : "";
        throw std::runtime_error("Paystack " + std::to_string(static_cast<int>(resp->statusCode())) + ": " + message);
    }
    return body.at("data");
}

Task<Checkout> PaymentService::initialize(const CheckoutInput& input) {
    auto req = request(drogon::Post, "/transaction/initialize");
    nlohmann::json body{
        {"email", input.email},
        {"amount", input.amount},
        {"currency", input.currency},
        {"reference", input.reference},
        {"callback_url", callbackUrl_},
    };
    if (input.subaccount) body["subaccount"] = *input.subaccount;
    req->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    req->setBody(body.dump());

    const auto data = dataOf(co_await clients_.get()->sendRequestCoro(req, 15));
    co_return Checkout{data.at("authorization_url").get<std::string>(), data.value("access_code", "")};
}

Task<PaystackStatus> PaymentService::verify(const std::string& reference) {
    const auto data = dataOf(co_await clients_.get()->sendRequestCoro(request(drogon::Get, "/transaction/verify/" + reference), 15));
    co_return PaystackStatus{
        data.at("status").get<std::string>(),
        data.at("amount").get<int64_t>(),
        data.at("currency").get<std::string>(),
    };
}

bool PaymentService::validSignature(std::string_view body, std::string_view signature) const {
    if (!configured() || signature.empty()) return false;
    unsigned char mac[EVP_MAX_MD_SIZE];
    unsigned int length = 0;
    HMAC(EVP_sha512(), apiKey_.data(), static_cast<int>(apiKey_.size()),
         reinterpret_cast<const unsigned char*>(body.data()), body.size(), mac, &length);
    const std::string expected = toHex(mac, length);
    return signature.size() == expected.size() &&
           CRYPTO_memcmp(expected.data(), signature.data(), expected.size()) == 0;
}

void PaymentService::queueSettle(const std::string& reference) {
    bus_->Publish(PAYMENT_RECEIVED, PaymentReceived{reference});
}

Task<std::string> PaymentService::settle(const std::string& reference) {
    const auto order = co_await orders_->getByReference(reference);
    if (order.getValueOfStatus() != "pending") co_return order.getValueOfStatus();
    if (!order.getAuthorizationUrl()) co_return "pending";

    const auto remote = co_await verify(reference);

    if (remote.status == "failed" || remote.status == "reversed") {
        co_await orders_->markFailed(reference, "failed");
        co_return "failed";
    }
    if (remote.status != "success") co_return "pending";

    if (remote.amount != order.getValueOfAmount() || remote.currency != order.getValueOfCurrency()) {
        LOG_ERROR << "Order " << reference << " was paid " << remote.amount << " " << remote.currency << ", expected "
                  << order.getValueOfAmount() << " " << order.getValueOfCurrency() << ". It needs a refund.";
        co_await orders_->markFailed(reference, "failed");
        co_return "failed";
    }

    std::optional<Orders> paid;
    bool alreadyPaid = false;
    try {
        co_await withTransaction(dbClient_, [&](DbClientPtr tx) -> Task<> {
            paid = co_await orders_->markPaid(reference, tx);
            if (paid && paid->getEventId() && paid->getUserId()) {
                co_await events_->addAttendee(paid->getValueOfEventId(), paid->getValueOfUserId(), tx);
            }
        });
    } catch (const drogon::orm::Failure& e) {
        if (!isUniqueViolation(e)) throw;
        alreadyPaid = true;
    }
    if (alreadyPaid) {
        LOG_ERROR << "Order " << reference << " was paid, but this attendee already paid for the event. It needs a refund.";
        co_await orders_->markFailed(reference, "failed");
        co_return "failed";
    }

    if (!paid) co_return (co_await orders_->getByReference(reference)).getValueOfStatus();

    if (paid->getEventId()) {
        bool exists = false;
        if (const auto host = co_await events_->hostOf(paid->getValueOfEventId(), exists)) {
            bus_->Publish(ATTENDEE_REGISTERED, AttendeeRegistered{paid->getValueOfEventId(), *host, paid->getValueOfEmail()});
        }
    }
    co_return "success";
}
