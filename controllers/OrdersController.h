#pragma once

#include <drogon/HttpController.h>

#include "services/Services.hpp"

#include "Api.hpp"

using namespace drogon;

class OrdersController : public drogon::HttpController<OrdersController, false>
{
  public:

    explicit OrdersController(Services* srv) : services_(srv) {}

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(OrdersController::ListOrders, API_V1 + "/orders/", Get, "JwtMiddleWare");
    ADD_METHOD_VIA_REGEX(OrdersController::GetOrder, API_V1 + "/orders/([0-9a-fA-F-]{36})/?", Get, "JwtMiddleWare");
    ADD_METHOD_VIA_REGEX(OrdersController::Checkout, API_V1 + "/orders/([0-9a-fA-F-]{36})/checkout/?", Post, "JwtMiddleWare");
    METHOD_LIST_END

    Task<HttpResponsePtr> ListOrders(HttpRequestPtr req);
    Task<HttpResponsePtr> GetOrder(HttpRequestPtr req, std::string id);
    Task<HttpResponsePtr> Checkout(HttpRequestPtr req, std::string id);

  private:
    Services* services_;
};
