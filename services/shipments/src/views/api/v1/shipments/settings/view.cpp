#include <helpers/employees.hpp>
#include <helpers/shipment_card.hpp>
#include <views/api/v1/shipments/settings/view.hpp>

namespace shipments::views::api::v1::shipments::settings {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    try {
        const auto organization =
            platform::auth::GetOrganization(platform::auth::GetIdentity(context));
        auto settings = GetShipmentSettings(deps, organization, request.body.customer_id);
        if (request.body.customer_id.has_value())
            settings.DefaultLogist =
                CustomerLogist(deps, context, request.body.customer_id.value());
        return Response200{.container_types = settings.ContainerTypes,
            .default_logist = settings.DefaultLogist,
            .number_template = settings.NumberTemplate,
            .planned_containers = settings.PlannedContainers,
            .reset_number_yearly = settings.ResetNumberYearly,
            .route_types = settings.RouteTypes,
            .statuses = settings.Statuses};
    } catch (const ::shipments::ApiError& error) {
        return Response503{.error = error.Code, .message = error.what()};
    }
}
}  // namespace shipments::views::api::v1::shipments::settings
