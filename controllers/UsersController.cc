#include "UsersController.h"
#include "input/UserInputs.hpp"
#include "utils/RequestBody.hpp"
#include "utils/Errors.hpp"
#include "utils/Pagination.hpp"

Task<HttpResponsePtr> UsersController::ListUsers(HttpRequestPtr) {
    co_return res::success(k200OK, "succesfully listed", co_await services_->userService->allUsersJson());
}

Task<HttpResponsePtr> UsersController::GetCurrentUser(HttpRequestPtr req) {
    const auto id = req->attributes()->get<std::string>("id");
    co_return res::success(k200OK, "User Retrieved Successfully", co_await services_->userService->userJson(id));
}

Task<HttpResponsePtr> UsersController::ListMyEvents(HttpRequestPtr req) {
    const auto page = parsePageQuery(req->getParameter("limit"), req->getParameter("offset"), 6);
    UserEventsFilter filter;
    filter.past = req->getParameter("when") == "past";
    const auto role = req->getParameter("role");
    if (role == "hosting") filter.role = UserEventsFilter::Role::Hosting;
    else if (role == "attending") filter.role = UserEventsFilter::Role::Attending;
    const auto price = req->getParameter("price");
    if (price == "free") filter.price = UserEventsFilter::Price::Free;
    else if (price == "paid") filter.price = UserEventsFilter::Price::Paid;

    const auto userId = req->attributes()->get<std::string>("id");
    const auto result = co_await services_->eventService->userEvents(userId, filter, page);
    co_return pagination::response(req, page, result.count, result.events);
}

Task<HttpResponsePtr> UsersController::UpdateCurrentUser(HttpRequestPtr req) {
    const auto body = readBody(req);
    if (body.error) co_return res::validation({*body.error});

    auto parsed = parseUpdateUser(body);
    if (auto* errors = std::get_if<std::vector<FieldError>>(&parsed)) co_return res::validation(*errors);
    const auto& input = std::get<UpdateUserInput>(parsed);

    if (input.profileImg && !co_await services_->cloudinaryService->exists(*input.profileImg)) {
        co_return res::validation("profile_img", "no uploaded photo found with this public_id");
    }

    const auto id = req->attributes()->get<std::string>("id");
    co_await services_->userService->updateUser(id, input);
    co_return res::success(k200OK, "Update Successfull", co_await services_->userService->userJson(id));
}

Task<HttpResponsePtr> UsersController::DeleteCurrentUser(HttpRequestPtr req) {
    const auto id = req->attributes()->get<std::string>("id");
    co_await services_->userService->deleteUser(id);
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k204NoContent);
    co_return resp;
}
