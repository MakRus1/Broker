#include <components/settings.hpp>
#include <components/shipments.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/shipments/settings/view.hpp>

namespace shipments::views::api::v1::shipments::settings {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    const auto& access_token = platform::auth::GetAccessToken(context);

    const auto& customer = request.body.customer_id;
    auto settings = deps.settings->Effective(organization, customer);
    auto customer_logist = customer.has_value()
        ? deps.shipments->GetCustomerLogist(access_token, customer.value())
        : std::nullopt;

    return Response200{.container_types = std::move(settings.ContainerTypes),
        .customer_logist = std::move(customer_logist),
        .number_template = std::move(settings.NumberTemplate),
        .planned_containers = settings.PlannedContainers,
        .reset_number_yearly = settings.ResetNumberYearly,
        .route_types = std::move(settings.RouteTypes),
        .statuses = std::move(settings.Statuses)};
}
}  // namespace shipments::views::api::v1::shipments::settings
