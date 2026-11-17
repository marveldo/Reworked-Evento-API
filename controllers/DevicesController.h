#pragma once

#include <drogon/HttpController.h>

#include "services/Services.hpp"

#include "Api.hpp"

using namespace drogon;

class DevicesController : public drogon::HttpController<DevicesController, false>
{
  public:

    explicit DevicesController(Services* srv) : services_(srv) {}

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(DevicesController::ListDevices, API_V1 + "/auth/devices/", Get, "JwtMiddleWare");
    ADD_METHOD_VIA_REGEX(DevicesController::LogoutDevice, API_V1 + "/auth/logout-device/([^/]+)/?", Delete, "JwtMiddleWare");
    METHOD_LIST_END

    Task<HttpResponsePtr> ListDevices(HttpRequestPtr req);
    Task<HttpResponsePtr> LogoutDevice(HttpRequestPtr req, std::string id);

  private:
    Services* services_;
};
