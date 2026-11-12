#include <components/transportation_operations.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/transportation_operations/get/view.hpp>

namespace shipments::views::api::v1::transportation_operations::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    platform::auth::CheckPermission(context, permissions::SHIPMENTS_READ);
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    return Response200{.operation = deps.transportation_operations->GetCreationOperation(
                           organization, request.body.operation_id)};
}
}  // namespace shipments::views::api::v1::transportation_operations::get
