#include "UserService.hpp"
#include <drogon/orm/CoroMapper.h>
#include <drogon/orm/Exception.h>
#include "services/serializers/Serializers.hpp"
#include "utils/Security.hpp"
#include "utils/Errors.hpp"
#include "utils/Media.hpp"
#include "events/Events.hpp"
#include "models/DeviceInfo.h"

using namespace drogon::orm;
using drogon::Task;
using drogon_model::evento::DeviceInfo;

namespace {
    const char* EMAIL_TAKEN = "user with this email already exists.";

    void applyProfile(Users& user, const ProfileFields& profile) {
        if (profile.location)  user.setLocation(*profile.location);
        if (profile.website)   user.setWebsite(*profile.website);
        if (profile.facebook)  user.setFacebook(*profile.facebook);
        if (profile.twitter)   user.setTwitter(*profile.twitter);
        if (profile.instagram) user.setInstagram(*profile.instagram);
        if (profile.bio)       user.setBio(*profile.bio);
    }
}

Task<Users> UserService::getUserById(const std::string& id) {
    CoroMapper<Users> users(dbClient_);
    co_return co_await users.findByPrimaryKey(id);
}

Task<std::optional<Users>> UserService::findByEmail(const std::string& email) {
    CoroMapper<Users> users(dbClient_);
    auto found = co_await users.findBy(Criteria(Users::Cols::_email, CompareOperator::EQ, email));
    if (found.empty()) co_return std::nullopt;
    co_return found.front();
}

Task<Users> UserService::createUser(const RegisterInput& input) {
    CoroMapper<Users> users(dbClient_);

    if (co_await findByEmail(input.email)) throw FieldConflict("email", EMAIL_TAKEN);

    Users user;
    user.setFullName(input.fullName);
    user.setEmail(input.email);
    user.setPassword(co_await security::hashAsync(input.password));
    applyProfile(user, input.profile);

    Users created;
    try {
        created = co_await users.insert(user);
    } catch (const UniqueViolation&) {
        throw FieldConflict("email", EMAIL_TAKEN);
    }

    events_->Publish(USER_CREATED, UserCreated{created.getValueOfId(), created.getValueOfEmail(), created.getValueOfFullName()});
    co_return created;
}

Task<std::pair<Users, bool>> UserService::findOrCreateSocialUser(const SocialProfile& profile) {
    if (auto existing = co_await findByEmail(profile.email)) co_return std::make_pair(*existing, false);

    CoroMapper<Users> users(dbClient_);
    Users user;
    user.setFullName(profile.fullName);
    user.setEmail(profile.email);
    user.setAuthProvider(profile.provider);
    if (!profile.picture.empty()) user.setProfileImg(profile.picture);
    user.setIsVerified(true);
    user.setPassword(security::unusablePassword());

    try {
        auto created = co_await users.insert(user);
        events_->Publish(USER_CREATED, UserCreated{created.getValueOfId(), created.getValueOfEmail(), created.getValueOfFullName()});
        co_return std::make_pair(created, true);
    } catch (const UniqueViolation&) {
    }
    auto existing = co_await findByEmail(profile.email);
    if (!existing) throw std::runtime_error("could not create or find the account for " + profile.email);
    co_return std::make_pair(*existing, false);
}

Task<Users> UserService::updateUser(const std::string& id, const UpdateUserInput& input) {
    CoroMapper<Users> users(dbClient_);
    auto user = co_await getUserById(id);
    const std::string oldPicture = user.getValueOfProfileImg();

    if (input.fullName) user.setFullName(*input.fullName);
    if (input.email && *input.email != user.getValueOfEmail()) {
        if (co_await findByEmail(*input.email)) throw FieldConflict("email", EMAIL_TAKEN);
        user.setEmail(*input.email);
    }
    if (input.password)   user.setPassword(co_await security::hashAsync(*input.password));
    if (input.profileImg) user.setProfileImg(media::photoUrl(*input.profileImg));
    applyProfile(user, input.profile);

    try {
        co_await users.update(user);
    } catch (const UniqueViolation&) {
        throw FieldConflict("email", EMAIL_TAKEN);
    }

    if (const auto oldId = media::photoIdFromUrl(oldPicture); !oldId.empty() && oldPicture != user.getValueOfProfileImg()) {
        events_->Publish(PHOTO_UNUSED, PhotoUnused{oldId});
    }
    co_return co_await getUserById(id);
}

Task<std::vector<std::string>> UserService::deleteUser(const std::string& id) {
    auto user = co_await getUserById(id);

    const auto devices = co_await CoroMapper<DeviceInfo>(dbClient_).findBy(Criteria(DeviceInfo::Cols::_user_id, id));
    std::vector<std::string> deviceIds;
    for (const auto& device : devices) deviceIds.push_back(device.getValueOfId());

    CoroMapper<Users> users(dbClient_);
    co_await users.deleteByPrimaryKey(id);

    for (const auto& deviceId : deviceIds) events_->Publish(DEVICE_LOGGED_OUT, DeviceLoggedOut{deviceId});
    if (const auto photo = media::photoIdFromUrl(user.getValueOfProfileImg()); !photo.empty()) {
        events_->Publish(PHOTO_UNUSED, PhotoUnused{photo});
    }
    co_return deviceIds;
}

Task<bool> UserService::checkPassword(const Users& user, const std::string& password) {
    const std::string stored = user.getValueOfPassword();
    if (!co_await security::verifyAsync(password, stored)) co_return false;

    if (security::needsRehash(stored)) {
        const std::string upgraded = co_await security::hashAsync(password);
        const std::vector<std::string> passwordColumn{Users::Cols::_password};
        co_await CoroMapper<Users>(dbClient_).updateBy(
            passwordColumn,
            Criteria(Users::Cols::_id, user.getValueOfId()) && Criteria(Users::Cols::_password, stored),
            upgraded);
    }
    co_return true;
}

Task<> UserService::setLastLogin(const std::string& id) {
    co_await dbClient_->execSqlCoro("UPDATE users SET last_login = NOW() WHERE id = $1", id);
}

Task<> UserService::setLocation(const std::string& id, const std::string& location) {
    const std::vector<std::string> locationColumn{Users::Cols::_location};
    co_await CoroMapper<Users>(dbClient_).updateBy(locationColumn, Criteria(Users::Cols::_id, id), location);
}

Task<Json::Value> UserService::userJson(const std::string& id) {
    const std::vector<std::string> ids{id};
    auto list = co_await usersJson(ids);
    if (list.empty()) throw NotFound("Not found.");
    co_return list[0];
}

Task<Json::Value> UserService::usersJson(const std::vector<std::string>& ids) {
    co_return co_await serializers::users(dbClient_, ids, frontendLink_);
}

Task<Json::Value> UserService::allUsersJson() {
    const auto users = co_await CoroMapper<Users>(dbClient_).orderBy(Users::Cols::_created_at).orderBy(Users::Cols::_id).findAll();
    std::vector<std::string> ids;
    for (const auto& user : users) ids.push_back(user.getValueOfId());
    co_return co_await serializers::users(dbClient_, ids, frontendLink_);
}
