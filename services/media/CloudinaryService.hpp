#pragma once
#include "utils/HttpClientPool.hpp"
#include <drogon/utils/coroutine.h>
#include <atomic>
#include <string>
#include <utility>
#include <vector>

using Params = std::vector<std::pair<std::string, std::string>>;

struct UploadSignature {
    std::string uploadUrl;
    Params fields;
};

class CloudinaryService {
public:
    CloudinaryService(std::string cloudName, std::string apiKey, std::string apiSecret,
                      std::string apiUrl = "https://api.cloudinary.com");

    UploadSignature signUpload(const std::string& publicId, const std::string& transformation) const;

    drogon::Task<bool> exists(std::string publicId);

    drogon::Task<> deleteByPrefix(std::string prefix);

    static std::string sign(Params params, const std::string& secret);

    static constexpr const char* PROFILE_SIZE = "c_limit,w_800,h_800";
    static constexpr const char* EVENT_SIZE   = "c_limit,w_1600,h_1600";

private:
    std::string cloudName_;
    std::string apiKey_;
    std::string apiSecret_;
    HttpClientPool delivery_;
    HttpClientPool api_;

    static constexpr const char* ALLOWED_FORMATS = "jpg,jpeg,png,webp";
};
