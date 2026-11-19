#include <drogon/drogon.h>
#include <algorithm>
#include "settings/Settings.h"
#include "controllers/AuthController.h"
#include "controllers/UsersController.h"
#include "controllers/DevicesController.h"
#include "controllers/EventsController.h"
#include "controllers/PhotosController.h"
#include "controllers/NotificationsSocketController.h"
#include "controllers/NotificationsController.h"
#include "controllers/PaymentsController.h"
#include "controllers/OrdersController.h"
#include "services/Services.hpp"
#include "bootstrap/Bootstrap.hpp"
#include "utils/Errors.hpp"
#include "utils/ApiDocs.hpp"
#include "utils/RateLimit.hpp"
#include "middleware/JwtMiddleware.hpp"

using namespace drogon;
int main() {

    AppContext* ctx = nullptr;

    try {
        ctx = bootstrap();
    } catch (const std::exception& e) {
        LOG_ERROR << "Failed to bootstrap application: " << e.what();
        return 1;
    }
    drogon::app().addListener(ctx->settings->host, ctx->settings->port);
    LOG_INFO << "Starting server on port " << ctx->settings->port << " with " << ctx->settings->threads << " threads in " << ctx->settings->environment << " mode.";
    drogon::app().setThreadNum(ctx->settings->threads);
    drogon::app().setClientMaxBodySize(1024 * 1024);

    const std::vector<std::string> origins = ctx->settings->corsAllowedOrigins;
    drogon::app().registerPreSendingAdvice([origins](const HttpRequestPtr &req, const HttpResponsePtr &resp) {
        const std::string& origin = req->getHeader("Origin");
        if (origins.empty()) {
            resp->addHeader("Access-Control-Allow-Origin", "*");
        } else if (std::find(origins.begin(), origins.end(), origin) != origins.end()) {
            resp->addHeader("Access-Control-Allow-Origin", origin);
            resp->addHeader("Vary", "Origin");
        }
        resp->addHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        resp->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");

        const auto us = trantor::Date::now().microSecondsSinceEpoch() - req->creationDate().microSecondsSinceEpoch();
        LOG_INFO << "Request: " << req->methodString() << " " << req->path() << ", Response: " << resp->statusCode()
                 << ", took " << us << "us";
    });
    drogon::app().registerPreRoutingAdvice([](const HttpRequestPtr& req, AdviceCallback&& reply, AdviceChainCallback&& pass) {
        if (req->method() != Options) {
            pass();
            return;
        }
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k204NoContent);
        reply(resp);
    });
    drogon::app().registerHandler("/", [](HttpRequestPtr req) -> Task<HttpResponsePtr> {
        Json::Value json;
        json["message"] = "Welcome to the Evento API!";
        json["status"]  = "success";
        co_return HttpResponse::newHttpJsonResponse(json);
    },{Get});

    registerApiDocs();         // GET /openapi.json and GET /docs
    registerUserRateLimit();   // 120 requests a minute per logged-in user (route limits: middleware/RateLimitMiddlewares)

    Services* service = getServices(ctx->db, *ctx->settings, ctx->events);
    drogon::app().registerMiddleware(std::make_shared<JwtMiddleWare>(service));
    drogon::app().registerMiddleware(std::make_shared<OptionalJwtMiddleWare>(service));
    drogon::app().registerController(std::make_shared<AuthController>(service));
    drogon::app().registerController(std::make_shared<UsersController>(service));
    drogon::app().registerController(std::make_shared<DevicesController>(service));
    drogon::app().registerController(std::make_shared<EventsController>(service));
    drogon::app().registerController(std::make_shared<PhotosController>(service));
    drogon::app().registerController(std::make_shared<NotificationsController>(service));
    drogon::app().registerController(std::make_shared<PaymentsController>(service));
    drogon::app().registerController(std::make_shared<OrdersController>(service));
    drogon::app().registerController(std::make_shared<NotificationsSocketController>(service));
    drogon::app().registerController(std::make_shared<HomeSocketController>(service));
    drogon::app().setExceptionHandler(handleException);
    drogon::app().setCustom404Page(res::notFound("Route not found"));
    drogon::app().run();

    delete ctx;
    return 0;
}
