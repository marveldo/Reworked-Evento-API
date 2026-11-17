#include "PhotosController.h"
#include "utils/Media.hpp"
#include "utils/RequestBody.hpp"
#include <drogon/utils/Utilities.h>

Task<HttpResponsePtr> PhotosController::UploadSignature(HttpRequestPtr req) {
    const auto body = readBody(req);
    if (body.error) co_return res::validation({*body.error});

    const std::string kindText = body.fields.contains("kind") && body.fields["kind"].is_string()
                                     ? body.fields["kind"].get<std::string>() : "";
    if (kindText != "profile" && kindText != "event") {
        co_return res::validation("kind", "must be one of: profile, event");
    }
    const media::PhotoKind kind = kindText == "profile" ? media::PhotoKind::Profile : media::PhotoKind::Event;

    const std::string publicId = media::photoPrefix(kind) + drogon::utils::getUuid();
    const auto signature = services_->cloudinaryService->signUpload(
        publicId, kind == media::PhotoKind::Profile ? CloudinaryService::PROFILE_SIZE : CloudinaryService::EVENT_SIZE);

    Json::Value data;
    data["upload_url"] = signature.uploadUrl;
    data["public_id"]  = publicId;
    data["kind"]       = kindText;
    data["fields"]     = Json::Value(Json::objectValue);
    for (const auto& [key, value] : signature.fields) data["fields"][key] = value;
    co_return res::success(k200OK, "Upload signature created", data);
}
