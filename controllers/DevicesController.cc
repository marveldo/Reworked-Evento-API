#include "DevicesController.h"
#include <regex>

Task<HttpResponsePtr> DevicesController::ListDevices(HttpRequestPtr req) {
    const auto userId = req->attributes()->get<std::string>("id");
    co_return res::success(k200OK, "Devices retrieved Successfully", co_await services_->deviceService->listForUser(userId));
}

Task<HttpResponsePtr> DevicesController::LogoutDevice(HttpRequestPtr req, std::string id) {
    static const std::regex uuid(R"([0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12})");
    const auto userId = req->attributes()->get<std::string>("id");
    if (!std::regex_match(id, uuid) || !co_await services_->deviceService->endSession(id, userId)) {
        co_return res::notFound("No DeviceInfo matches the given query.");
    }
    co_return res::success(k200OK, "Device logged out", Json::Value(Json::objectValue));
}
