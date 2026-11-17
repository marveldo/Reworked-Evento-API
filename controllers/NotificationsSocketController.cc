#include "NotificationsSocketController.h"
#include "middleware/JwtMiddleware.hpp"
#include "settings/Settings.h"
#include <algorithm>
#include <json/json.h>

namespace {
    // The close code the frontend treats as "you've been logged out".
    const CloseCode LOGGED_OUT = static_cast<CloseCode>(4401);

    // What a socket is listening to, so it can stop when it closes.
    struct SocketSession {
        std::string userId;
        std::string deviceId;
        uint64_t notificationsListener = 0;
        uint64_t logoutListener = 0;
    };

    // Like Channels' OriginValidator with CORS_ALLOWED_ORIGINS: only the frontend's pages may connect.
    // With CORS_ALLOWED_ORIGINS empty, any origin may.
    bool allowedOrigin(const HttpRequestPtr& req) {
        const auto& allowed = getSettings()->corsAllowedOrigins;
        if (allowed.empty()) return true;
        std::string origin = req->getHeader("Origin");
        while (!origin.empty() && origin.back() == '/') origin.pop_back();
        return std::find(allowed.begin(), allowed.end(), origin) != allowed.end();
    }

    std::string compact(const Json::Value& value) {
        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";
        return Json::writeString(writer, value);
    }
}

Task<> NotificationsSocketController::handleNewConnectionCoro(const HttpRequestPtr& req, const WebSocketConnectionPtr& conn) {
    if (!allowedOrigin(req)) {
        conn->shutdown(CloseCode::kViolation, "Origin not allowed");
        co_return;
    }
    const auto claims = co_await JwtMiddleWare::authenticate(services_, req->getParameter("access_token"));
    if (!claims) {
        conn->shutdown(LOGGED_OUT, "Authentication error");
        co_return;
    }

    auto session = std::make_shared<SocketSession>();
    session->userId   = claims->userId;
    session->deviceId = claims->deviceId;
    conn->setContext(session);

    // The listeners hold a weak pointer: a closed socket isn't kept alive by them.
    std::weak_ptr<WebSocketConnection> weak = conn;
    RealtimeService* realtime = services_->realtimeService;
    session->notificationsListener = realtime->listen(RealtimeService::notificationsChannel(session->userId),
        [weak](const std::string& message) {
            if (auto socket = weak.lock(); socket && socket->connected()) socket->send(message);
        });
    session->logoutListener = realtime->listen(RealtimeService::logoutChannel(session->deviceId),
        [weak](const std::string&) {
            if (auto socket = weak.lock(); socket && socket->connected()) socket->shutdown(LOGGED_OUT, "This device has been logged Out");
        });

    // Logged out between the token check and the subscription: that message was missed, so check again.
    if (!co_await services_->deviceService->isActive(session->deviceId, session->userId)) {
        conn->shutdown(LOGGED_OUT, "This device has been logged Out");
    }
}

Task<> NotificationsSocketController::handleNewMessageCoro(const WebSocketConnectionPtr&, std::string&&, const WebSocketMessageType&) {
    co_return;   // the client only listens; pings are answered by Drogon
}

Task<> NotificationsSocketController::handleConnectionClosedCoro(const WebSocketConnectionPtr& conn) {
    if (auto session = conn->getContext<SocketSession>()) {
        services_->realtimeService->stopListening(RealtimeService::notificationsChannel(session->userId), session->notificationsListener);
        services_->realtimeService->stopListening(RealtimeService::logoutChannel(session->deviceId), session->logoutListener);
    }
    co_return;
}

Task<> HomeSocketController::handleNewConnectionCoro(const HttpRequestPtr& req, const WebSocketConnectionPtr& conn) {
    if (!allowedOrigin(req)) {
        conn->shutdown(CloseCode::kViolation, "Origin not allowed");
        co_return;
    }
    const auto claims = co_await JwtMiddleWare::authenticate(services_, req->getParameter("access_token"));
    if (!claims) {
        conn->shutdown(LOGGED_OUT, "Unauthenticated");
        co_return;
    }
    Json::Value hello;
    hello["message"] = "Hello welcome to the websocket " + claims->email;
    hello["detail"]  = "Connection Successful";
    conn->send(compact(hello));
}

Task<> HomeSocketController::handleNewMessageCoro(const WebSocketConnectionPtr&, std::string&&, const WebSocketMessageType&) {
    co_return;
}

Task<> HomeSocketController::handleConnectionClosedCoro(const WebSocketConnectionPtr&) {
    co_return;
}
