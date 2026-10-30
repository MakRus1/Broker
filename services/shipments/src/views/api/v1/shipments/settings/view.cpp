#include <helpers/employees.hpp>
#include <helpers/shipment_card.hpp>
#include <views/api/v1/shipments/settings/view.hpp>

namespace shipments::views::api::v1::shipments::settings {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;

    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    const auto settings = GetShipmentSettings(deps, organization, request.body.customer_id);
    const auto customer_logist = request.body.customer_id.has_value()
        ? CustomerLogist(deps, context, request.body.customer_id.value())
        : std::nullopt;
    return Response200{.container_types = settings.ContainerTypes,
        .customer_logist = customer_logist,
        .number_template = settings.NumberTemplate,
        .planned_containers = settings.PlannedContainers,
        .reset_number_yearly = settings.ResetNumberYearly,
        .route_types = settings.RouteTypes,
        .statuses = settings.Statuses};
}
}  // namespace shipments::views::api::v1::shipments::settings
