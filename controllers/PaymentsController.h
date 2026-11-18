#pragma once

#include <drogon/HttpController.h>

#include "services/Services.hpp"

#include "Api.hpp"

using namespace drogon;

class PaymentsController : public drogon::HttpController<PaymentsController, false>
{
  public:

    explicit PaymentsController(Services* srv) : services_(srv) {}

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(PaymentsController::Webhook, API_V1 + "/payments/webhook/", Post);
    ADD_METHOD_VIA_REGEX(PaymentsController::Verify, API_V1 + "/payments/([A-Za-z0-9_-]{1,64})/verify/?", Post, "JwtMiddleWare");
    METHOD_LIST_END

    Task<HttpResponsePtr> Webhook(HttpRequestPtr req);
    Task<HttpResponsePtr> Verify(HttpRequestPtr req, std::string reference);

  private:
    Services* services_;
};
