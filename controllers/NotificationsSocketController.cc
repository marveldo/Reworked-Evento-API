#include "NotificationsSocketController.h"
#include "middleware/JwtMiddleware.hpp"
#include "settings/Settings.h"
#include <drogon/drogon.h>
#include <algorithm>
#include <json/json.h>

namespace {
    // The close code the frontend treats as "you've been logged out".
    const CloseCode LOGGED_OUT = static_cast<CloseCode>(4401);
    const CloseCode BAD_TOKEN = static_cast<CloseCode>(4403);

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

    enum class SocketAuth { Ok, BadToken, LoggedOut };

    Task<std::pair<SocketAuth, std::shared_ptr<Claims>>> authenticateSocket(Services* services, const HttpRequestPtr& req) {
        const std::string socketToken = req->getParameter("token");
        if (socketToken.empty()) {
            auto claims = co_await JwtMiddleWare::authenticate(services, req->getParameter("access_token"));
            co_return std::make_pair(claims ? SocketAuth::Ok : SocketAuth::BadToken, claims);
        }

        std::shared_ptr<Claims> claims;
        try {
            claims = services->jwtService->verifySocketToken(socketToken);
        } catch (const std::exception&) {
            co_return std::make_pair(SocketAuth::BadToken, nullptr);
        }
        if (!co_await services->deviceService->isActive(claims->deviceId, claims->userId)) {
            co_return std::make_pair(SocketAuth::LoggedOut, nullptr);
        }
        co_return std::make_pair(SocketAuth::Ok, claims);
    }

    constexpr double CLOSE_GRACE_SECONDS = 1.0;

    void closeWith(const WebSocketConnectionPtr& conn, CloseCode code, const std::string& reason) {
        conn->shutdown(code, reason);
        std::weak_ptr<WebSocketConnection> weak = conn;
        drogon::app().getLoop()->runAfter(CLOSE_GRACE_SECONDS, [weak]() {
            if (auto socket = weak.lock(); socket && !socket->disconnected()) socket->forceClose();
        });
    }

    std::string compact(const Json::Value& value) {
        Json::StreamWriterBuilder writer;
        writer["indentation"] = "";
        return Json::writeString(writer, value);
    }

    const std::string LOGGED_OUT_REASON = "This device has been logged Out";

    void logOut(const WebSocketConnectionPtr& conn) {
        Json::Value message;
        message["type"]   = "logout";
        message["detail"] = LOGGED_OUT_REASON;
        if (conn->connected()) conn->send(compact(message));
        closeWith(conn, LOGGED_OUT, LOGGED_OUT_REASON);
    }

    void reject(const WebSocketConnectionPtr& conn, SocketAuth result) {
        if (result == SocketAuth::LoggedOut) logOut(conn);
        else closeWith(conn, BAD_TOKEN, "Invalid or expired token");
    }
}

Task<> NotificationsSocketController::handleNewConnectionCoro(const HttpRequestPtr& req, const WebSocketConnectionPtr& conn) {
    if (!allowedOrigin(req)) {
        closeWith(conn, CloseCode::kViolation, "Origin not allowed");
        co_return;
    }
    const auto [result, claims] = co_await authenticateSocket(services_, req);
    if (result != SocketAuth::Ok) {
        reject(conn, result);
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
            if (auto socket = weak.lock(); socket && socket->connected()) logOut(socket);
        });

    // Logged out between the token check and the subscription: that message was missed, so check again.
    if (!co_await services_->deviceService->isActive(session->deviceId, session->userId)) {
        logOut(conn);
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
        closeWith(conn, CloseCode::kViolation, "Origin not allowed");
        co_return;
    }
    const auto [result, claims] = co_await authenticateSocket(services_, req);
    if (result != SocketAuth::Ok) {
        reject(conn, result);
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
