#include <boost/uuid/uuid_io.hpp>
#include <components/container_plan.hpp>
#include <components/container_plan_calculation.hpp>
#include <models/errors.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/shipment_items/list/view.hpp>

namespace shipments::views::api::v1::shipment_items::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    platform::auth::CheckPermission(context, permissions::SHIPMENTS_READ);
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));

    const auto& body = request.body;
    const auto snapshot = deps.container_plan->LoadPlan(organization, body.shipment_id);

    namespace api = clients::transportations::ApiV1TransportationsContainerFact;
    api::RequestBody fact_request;
    fact_request.shipment_id = body.shipment_id;
    for (const auto& operation : snapshot.Pending)
        fact_request.pending_operation_ids.push_back(operation.Id);
    const auto facts = deps.transportations.ApiV1TransportationsContainerFact(
        {.body = std::move(fact_request)}, platform::auth::GetAccessToken(context));

    auto result = container_plan::BuildPlanPage(snapshot, facts, body.after, body.limit);
    return Response200{
        .items = std::move(result.items), .unplanned_items = std::move(result.unplanned_items)};
}
}  // namespace shipments::views::api::v1::shipment_items::list
