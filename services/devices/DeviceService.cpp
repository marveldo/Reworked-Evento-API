#include "DeviceService.hpp"
#include <drogon/orm/CoroMapper.h>
#include <drogon/utils/Utilities.h>
#include <trantor/utils/Logger.h>
#include "events/Events.hpp"

using namespace drogon::orm;
using drogon::Task;

namespace {
    template <typename Setter>
    void setIfPresent(const std::string& value, Setter set) {
        if (!value.empty()) set(value);
    }
}

Task<DeviceSession> DeviceService::startSession(const drogon_model::evento::Users& user, const DeviceDetailsInput& details) {
    DeviceSession session;
    session.userId   = user.getValueOfId();
    session.deviceId = drogon::utils::getUuid();

    Claims claims{user.getValueOfId(), "", user.getValueOfEmail(), session.deviceId};
    auto refreshToken = jwt_->generateRefreshToken(claims);
    auto accessToken = jwt_->generateAccessToken(claims);
    session.accessToken  = accessToken.token;
    session.refreshToken  = refreshToken.token;
    session.accessexpirytime = accessToken.expiry;
    session.refreshexpirytime = refreshToken.expiry;

    DeviceInfo device;
    device.setId(session.deviceId);
    device.setUserId(user.getValueOfId());
    setIfPresent(details.ipAddress,  [&](const std::string& v) { device.setIpAddress(v.substr(0, 120)); });
    setIfPresent(details.deviceName, [&](const std::string& v) { device.setDeviceName(v.substr(0, 120)); });
    setIfPresent(details.deviceOs,   [&](const std::string& v) { device.setDeviceOs(v.substr(0, 600)); });
    setIfPresent(details.deviceType, [&](const std::string& v) { device.setDeviceType(v.substr(0, 600)); });
    device.setAccessToken(session.accessToken);
    device.setRefreshToken(session.refreshToken);

    CoroMapper<DeviceInfo> devices(dbClient_);
    co_await devices.insert(device);

    events_->Publish(DEVICE_LOGGED_IN, DeviceLoggedIn{session.deviceId, user.getValueOfId(), details.ipAddress});
    co_return session;
}

Task<std::optional<DeviceSession>> DeviceService::refreshSession(const std::string& refreshToken) {
    std::shared_ptr<Claims> claims;
    try {
        claims = jwt_->verifyRefreshToken(refreshToken);
    } catch (const std::exception&) {
        
    }
    if (!claims){
        co_return std::nullopt;
    }

    DeviceSession session;
    session.userId       = claims->userId;
    session.deviceId     = claims->deviceId;
    auto newrefreshToken = jwt_->generateRefreshToken(*claims);
    auto newaccessToken = jwt_->generateAccessToken(*claims);
    session.accessToken  = newaccessToken.token;
    session.refreshToken  = newrefreshToken.token;
    session.accessexpirytime = newaccessToken.expiry;
    session.refreshexpirytime = newrefreshToken.expiry;


    const std::vector<std::string> tokenColumns{DeviceInfo::Cols::_access_token, DeviceInfo::Cols::_refresh_token};
    const size_t updated = co_await CoroMapper<DeviceInfo>(dbClient_).updateBy(
        tokenColumns,
        Criteria(DeviceInfo::Cols::_id, claims->deviceId) && Criteria(DeviceInfo::Cols::_user_id, claims->userId) &&
            Criteria(DeviceInfo::Cols::_refresh_token, refreshToken),
        session.accessToken, session.refreshToken);
    if (updated == 0) co_return std::nullopt;
    co_return session;
}

Task<bool> DeviceService::isActive(const std::string& deviceId, const std::string& userId) {
    co_return co_await CoroMapper<DeviceInfo>(dbClient_).count(
        Criteria(DeviceInfo::Cols::_id, deviceId) && Criteria(DeviceInfo::Cols::_user_id, userId)) > 0;
}

Task<bool> DeviceService::isCurrentAccessToken(const std::string& deviceId, const std::string& userId, const std::string& token) {
    co_return co_await CoroMapper<DeviceInfo>(dbClient_).count(
        Criteria(DeviceInfo::Cols::_id, deviceId) && Criteria(DeviceInfo::Cols::_user_id, userId) &&
        Criteria(DeviceInfo::Cols::_access_token, token)) > 0;
}

Task<Json::Value> DeviceService::listForUser(const std::string& userId) {
    CoroMapper<DeviceInfo> devices(dbClient_);
    const auto rows = co_await devices.orderBy(DeviceInfo::Cols::_created_at, SortOrder::DESC)
                                      .findBy(Criteria(DeviceInfo::Cols::_user_id, CompareOperator::EQ, userId));
    Json::Value list(Json::arrayValue);
    for (const auto& device : rows) {
        Json::Value item;
        const auto text = [](const std::shared_ptr<std::string>& value) { return value ? Json::Value(*value) : Json::Value(); };
        item["id"]          = device.getValueOfId();
        item["ip_address"]  = text(device.getIpAddress());
        item["device_name"] = text(device.getDeviceName());
        item["device_os"]   = text(device.getDeviceOs());
        item["device_type"] = text(device.getDeviceType());
        item["user"]        = device.getValueOfUserId();
        list.append(item);
    }
    co_return list;
}

Task<bool> DeviceService::endSession(const std::string& deviceId, const std::string& userId) {
    const size_t deleted = co_await CoroMapper<DeviceInfo>(dbClient_).deleteBy(
        Criteria(DeviceInfo::Cols::_id, deviceId) && Criteria(DeviceInfo::Cols::_user_id, userId));
    if (deleted == 0) co_return false;
    events_->Publish(DEVICE_LOGGED_OUT, DeviceLoggedOut{deviceId});
    co_return true;
}
