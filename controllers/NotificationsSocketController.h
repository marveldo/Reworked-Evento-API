#pragma once

#include <drogon/WebSocketController.h>

#include "services/Services.hpp"

using namespace drogon;

class NotificationsSocketController : public drogon::WebSocketCoroController<NotificationsSocketController, false>
{
  public:
    explicit NotificationsSocketController(Services* srv) : services_(srv) {}

    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/ws/notifications/");
    WS_PATH_LIST_END

    Task<> handleNewConnectionCoro(const HttpRequestPtr& req, const WebSocketConnectionPtr& conn) override;
    Task<> handleNewMessageCoro(const WebSocketConnectionPtr& conn, std::string&& message, const WebSocketMessageType& type) override;
    Task<> handleConnectionClosedCoro(const WebSocketConnectionPtr& conn) override;

  private:
    Services* services_;
};

class HomeSocketController : public drogon::WebSocketCoroController<HomeSocketController, false>
{
  public:
    explicit HomeSocketController(Services* srv) : services_(srv) {}

    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/ws/home/");
    WS_PATH_LIST_END

    Task<> handleNewConnectionCoro(const HttpRequestPtr& req, const WebSocketConnectionPtr& conn) override;
    Task<> handleNewMessageCoro(const WebSocketConnectionPtr& conn, std::string&& message, const WebSocketMessageType& type) override;
    Task<> handleConnectionClosedCoro(const WebSocketConnectionPtr& conn) override;

  private:
    Services* services_;
};
