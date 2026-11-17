#pragma once
#include "settings/Settings.h"
#include <regex>
#include <string>

namespace media {

enum class PhotoKind { Profile, Event };

inline std::string subfolder(PhotoKind kind) {
    return kind == PhotoKind::Profile ? "users" : "events";
}

inline std::string photoPrefix(PhotoKind kind) {
    return getSettings()->cloudinary.folder + "/" + subfolder(kind) + "/";
}

inline bool isPhotoId(const std::string& publicId, PhotoKind kind) {
    static const std::regex uuid(R"([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12})");
    const std::string prefix = photoPrefix(kind);
    return publicId.rfind(prefix, 0) == 0 && std::regex_match(publicId.substr(prefix.size()), uuid);
}

inline std::string photoUrl(const std::string& publicId) {
    return "https://res.cloudinary.com/" + getSettings()->cloudinary.cloudName + "/image/upload/f_auto,q_auto/" + publicId;
}

inline std::string photoIdFromUrl(const std::string& url) {
    const std::string base = photoUrl("");
    if (url.rfind(base, 0) != 0) return "";
    const std::string id = url.substr(base.size());
    return isPhotoId(id, PhotoKind::Profile) || isPhotoId(id, PhotoKind::Event) ? id : "";
}

}
