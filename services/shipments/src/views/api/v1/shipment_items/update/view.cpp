#include <components/container_plan.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/shipment_items/update/view.hpp>

namespace shipments::views::api::v1::shipment_items::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    platform::auth::CheckPermission(context, permissions::SHIPMENTS_WRITE);
    const auto& actor = platform::auth::GetIdentity(context);
    return Response200{.item = deps.container_plan->UpdatePlannedContainer(request.body, actor)};
}
}  // namespace shipments::views::api::v1::shipment_items::update
