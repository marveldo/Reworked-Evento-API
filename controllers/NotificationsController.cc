#include "NotificationsController.h"
#include "input/QueryInputs.hpp"
#include "utils/Pagination.hpp"

Task<HttpResponsePtr> NotificationsController::ListNotifications(HttpRequestPtr req) {
    const auto page   = parsePageQuery(req->getParameter("limit"), req->getParameter("offset"), 20);
    const auto userId = req->attributes()->get<std::string>("id");
    const auto result = co_await services_->notificationService->list(userId, page);
    co_return pagination::response(req, page, result.count, result.items);
}
