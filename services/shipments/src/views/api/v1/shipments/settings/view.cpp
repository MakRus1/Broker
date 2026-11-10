#include <components/settings.hpp>
#include <utility>
#include <views/api/v1/shipments/settings/view.hpp>

namespace shipments::views::api::v1::shipments::settings {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.settings->Get(request.body, context);
    return Response200{.container_types = std::move(result.container_types),
        .customer_logist = std::move(result.customer_logist),
        .number_template = std::move(result.number_template),
        .planned_containers = std::move(result.planned_containers),
        .reset_number_yearly = std::move(result.reset_number_yearly),
        .route_types = std::move(result.route_types),
        .statuses = std::move(result.statuses)};
}
}  // namespace shipments::views::api::v1::shipments::settings
