#include <helpers/inherited_route.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/transportations/get/view.hpp>

namespace transportations::views::api::v1::transportations::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::transportations::helpers;

    platform::auth::CheckPermission(context, permissions::TRANSPORTATIONS_READ);
    const auto result = LoadTransportation(deps, context, request.body.transportation_id);
    return Response200{.transportation = result};
}
}  // namespace transportations::views::api::v1::transportations::get
