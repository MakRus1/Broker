#include <components/transportation_operations.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/transportation_operations/create/view.hpp>

namespace shipments::views::api::v1::transportation_operations::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    platform::auth::CheckPermission(context, permissions::SHIPMENTS_WRITE);
    const auto& actor = platform::auth::GetIdentity(context);
    return Response202{
        .operation = deps.transportation_operations->RequestTransportations(actor, request.body)};
}
}  // namespace shipments::views::api::v1::transportation_operations::create
