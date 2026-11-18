#include "OrdersController.h"
#include "input/QueryInputs.hpp"
#include "utils/Errors.hpp"
#include "utils/Pagination.hpp"

Task<HttpResponsePtr> OrdersController::ListOrders(HttpRequestPtr req) {
    const auto page   = parsePageQuery(req->getParameter("limit"), req->getParameter("offset"), 20);
    const auto userId = req->attributes()->get<std::string>("id");
    const auto result = co_await services_->orderService->listForUser(userId, page);

    co_return pagination::response(req, page, result.count, co_await services_->orderService->withEvents(result.items));
}

Task<HttpResponsePtr> OrdersController::GetOrder(HttpRequestPtr req, std::string id) {
    const auto userId = req->attributes()->get<std::string>("id");
    const auto order  = co_await services_->orderService->getById(id);
    if (order.getValueOfUserId() != userId) throw ::NotFound("Order not found");
    const std::vector<Orders> one{order};
    const auto items = co_await services_->orderService->withEvents(one);
    co_return res::success(k200OK, "Order", items[0]);
}

Task<HttpResponsePtr> OrdersController::Checkout(HttpRequestPtr req, std::string id) {
    const auto userId = req->attributes()->get<std::string>("id");
    const auto order  = co_await services_->orderService->getById(id);
    if (order.getValueOfUserId() != userId) throw ::NotFound("Order not found");
    if (order.getValueOfStatus() != "pending") co_return res::error(k400BadRequest, "This order is already " + order.getValueOfStatus() + ".");
    if (!order.getEventId()) co_return res::error(k400BadRequest, "This event is no longer available.");

    const auto registration = co_await services_->eventService->checkRegistration(order.getValueOfEventId(), order.getValueOfEmail());
    if (registration.check == RegistrationCheck::AlreadyRegistered) co_return res::error(k400BadRequest, "You're already registered for this event.");

    Json::Value data;
    data["reference"] = order.getValueOfReference();
    if (order.getAuthorizationUrl()) {
        data["authorization_url"] = order.getValueOfAuthorizationUrl();
        co_return res::success(k200OK, "Checkout", data);
    }

    auto& payments = *services_->paymentService;
    if (!payments.configured()) co_return res::error(k503ServiceUnavailable, "Payments aren't available right now.");

    ::Checkout checkout;
    bool started = false;
    try {
        checkout = co_await payments.initialize({order.getValueOfEmail(), order.getValueOfAmount(), order.getValueOfCurrency(),
                                                 order.getValueOfReference()});
        started = true;
    } catch (const std::exception& e) {
        LOG_WARN << "Paystack checkout for " << order.getValueOfReference() << " failed: " << e.what();
    }
    if (!started) co_return res::error(k502BadGateway, "Couldn't start the payment with Paystack right now. Try again shortly.");

    co_await services_->orderService->setAuthorizationUrl(order.getValueOfId(), checkout.authorizationUrl);
    data["authorization_url"] = checkout.authorizationUrl;
    co_return res::success(k200OK, "Checkout", data);
}
