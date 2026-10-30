#include <models/errors.hpp>
#include <operations/request_transportations.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/transportation_operations/create/view.hpp>

namespace shipments::views::api::v1::transportation_operations::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {

    platform::auth::CheckPermission(context, permissions::SHIPMENTS_WRITE);
    const auto result = ::shipments::operations::RequestTransportations(
        deps.pg_cluster, platform::auth::GetIdentity(context), request.body);
    return Response202{.operation = result};
}
}  // namespace shipments::views::api::v1::transportation_operations::create
