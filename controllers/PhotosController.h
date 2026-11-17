#pragma once

#include <drogon/HttpController.h>

#include "services/Services.hpp"

#include "Api.hpp"

using namespace drogon;

class PhotosController : public drogon::HttpController<PhotosController, false>
{
  public:

    explicit PhotosController(Services* srv) : services_(srv) {}

    METHOD_LIST_BEGIN
    ADD_METHOD_TO(PhotosController::UploadSignature, API_V1 + "/photos/signature/", Post, "JwtMiddleWare", "SignatureRateLimit");
    METHOD_LIST_END

    Task<HttpResponsePtr> UploadSignature(HttpRequestPtr req);

  private:
    Services* services_;
};
