#include "PaymentsController.h"
#include "utils/Errors.hpp"
#include <nlohmann/json.hpp>

Task<HttpResponsePtr> PaymentsController::Webhook(HttpRequestPtr req) {
    auto& payments = *services_->paymentService;
    if (!payments.validSignature(req->body(), req->getHeader("x-paystack-signature"))) {
        co_return res::error(k401Unauthorized, "Bad signature");
    }

    const auto event = nlohmann::json::parse(req->body(), nullptr, false);
    if (!event.is_object() || !event.contains("data") || !event["data"].is_object()) {
        co_return res::error(k400BadRequest, "Bad webhook body");
    }

    if (event.value("event", "") == "charge.success") {
        const std::string reference = event["data"].value("reference", "");
        if (!reference.empty()) payments.queueSettle(reference);
    }
    co_return HttpResponse::newHttpResponse();
}

Task<HttpResponsePtr> PaymentsController::Verify(HttpRequestPtr req, std::string reference) {
    const auto userId = req->attributes()->get<std::string>("id");
    const auto order  = co_await services_->orderService->getByReference(reference);
    if (order.getValueOfUserId() != userId) throw ::NotFound("Order not found");

    std::optional<std::string> failure;
    try {
        co_await services_->paymentService->settle(reference);
    } catch (const PaymentsNotConfigured&) {
        failure = "Payments aren't available right now.";
    } catch (const AppError&) {
        throw;
    } catch (const std::exception& e) {
        LOG_WARN << "verify " << reference << ": " << e.what();
        failure = "Couldn't confirm the payment with Paystack right now. Try again shortly.";
    }
    if (failure) co_return res::error(k502BadGateway, *failure);

    const auto settled = co_await services_->orderService->getByReference(reference);
    co_return res::success(k200OK, "Order " + settled.getValueOfStatus(), OrderService::toJson(settled));
}
