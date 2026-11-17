#include "CloudinaryService.hpp"
#include <drogon/utils/Utilities.h>
#include <json/json.h>
#include <openssl/evp.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <stdexcept>

namespace {
    std::string nowSeconds() {
        const auto now = std::chrono::system_clock::now().time_since_epoch();
        return std::to_string(std::chrono::duration_cast<std::chrono::seconds>(now).count());
    }

    // Cloudinary errors look like {"error":{"message":"..."}}.
    std::string errorOf(const drogon::HttpResponsePtr& resp) {
        if (auto json = resp->getJsonObject(); json && (*json)["error"].isObject()) {
            return (*json)["error"]["message"].asString();
        }
        return "HTTP " + std::to_string(static_cast<int>(resp->statusCode()));
    }
}

CloudinaryService::CloudinaryService(std::string cloudName, std::string apiKey, std::string apiSecret, std::string apiUrl)
    : cloudName_(std::move(cloudName)),
      apiKey_(std::move(apiKey)),
      apiSecret_(std::move(apiSecret)),
      delivery_(drogon::HttpClient::newHttpClient("https://res.cloudinary.com")),
      api_(drogon::HttpClient::newHttpClient(apiUrl)) {}

std::string CloudinaryService::sign(Params params, const std::string& secret) {
    std::sort(params.begin(), params.end());

    std::string toSign;
    for (size_t i = 0; i < params.size(); i++) {
        if (i > 0) toSign += '&';
        toSign += params[i].first + "=" + params[i].second;
    }
    toSign += secret;

    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int length = 0;
    EVP_Digest(toSign.data(), toSign.size(), digest, &length, EVP_sha1(), nullptr);

    std::string hex;
    char byte[3];
    for (unsigned int i = 0; i < length; i++) {
        std::snprintf(byte, sizeof(byte), "%02x", digest[i]);
        hex += byte;
    }
    return hex;
}

UploadSignature CloudinaryService::signUpload(const std::string& publicId, const std::string& transformation) const {
    Params params = {
        {"allowed_formats", ALLOWED_FORMATS},
        {"public_id", publicId},
        {"timestamp", nowSeconds()},
        {"transformation", transformation},
    };

    UploadSignature result;
    result.uploadUrl = "https://api.cloudinary.com/v1_1/" + cloudName_ + "/image/upload";
    result.fields = params;
    result.fields.push_back({"api_key", apiKey_});
    result.fields.push_back({"signature", sign(params, apiSecret_)});
    return result;
}

drogon::Task<bool> CloudinaryService::exists(std::string publicId) {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->setMethod(drogon::Head);
    req->setPath("/" + cloudName_ + "/image/upload/" + publicId);
    auto resp = co_await delivery_->sendRequestCoro(req, 10);
    co_return resp->statusCode() == drogon::k200OK;
}

// Admin API. Deletes up to 1000 images per call and says `partial` when there are more.
drogon::Task<> CloudinaryService::deleteByPrefix(std::string prefix) {
    const std::string credentials = drogon::utils::base64Encode(apiKey_ + ":" + apiSecret_);
    for (int round = 0; round < 20; round++) {
        auto req = drogon::HttpRequest::newHttpRequest();
        req->setMethod(drogon::Delete);
        req->setPath("/v1_1/" + cloudName_ + "/resources/image/upload");
        req->setParameter("prefix", prefix);
        req->addHeader("Authorization", "Basic " + credentials);

        auto resp = co_await api_->sendRequestCoro(req, 60);
        if (resp->statusCode() != drogon::k200OK) {
            throw std::runtime_error("Deleting images under " + prefix + " failed: " + errorOf(resp));
        }
        auto json = resp->getJsonObject();
        if (!json || !(*json)["partial"].asBool()) co_return;
    }
}
