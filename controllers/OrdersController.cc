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
