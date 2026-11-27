#include <components/actual_changes.hpp>
#include <components/actuals.hpp>
#include <components/inherited_route.hpp>
#include <components/routes.hpp>
#include <converters/route.hpp>
#include <helpers/revision.hpp>
#include <helpers/route_usage.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>

namespace transportations {
namespace pg = userver::storages::postgres;

ActualsComponent::ActualsComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-transportations")
                   .GetCluster()),
      routes_(context.FindComponent<RoutesComponent>()) {}

template <typename Request>
models::Transportation ActualsComponent::ApplyActualChange(const boost::uuids::uuid& organization,
    const platform::auth::Identity& identity, std::string_view access_token, const Request& request,
    const models::ShipmentContext& source, route_rules::ActualChange change,
    models::Transportation result) const {
    route_rules::InheritRoute(result, source);
    route_rules::ApplyActual(
        result, request.route_segment_id, request.event_kind, request.occurred_at, change);

    const auto usage_check_version =
        routes_.RegisterUse(organization, access_token, source, {request.route_segment_id});
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    helpers::CheckRouteWrite(transaction, organization, source.shipment_id, usage_check_version);
    const pg::ResultSet updated = transaction.Execute(sql::kUpdateRouteSegments, organization,
        result.id, request.expected_revision, converters::ToStoredRoute(result.route_segments));
    if (updated.IsEmpty())
        throw RevisionConflict{};
    result.revision = updated.AsSingleRow<std::int64_t>();

    helpers::WriteAudit(transaction, organization, identity,
        change == route_rules::ActualChange::Record ? "record_actual" : "correct_actual", result.id,
        request, result);
    transaction.Commit();
    return result;
}

models::Transportation ActualsComponent::RecordActual(const boost::uuids::uuid& organization,
    const platform::auth::Identity& actor, std::string_view access_token,
    const models::RecordActualRequest& request, const models::ShipmentContext& source,
    models::Transportation current) const {
    return ApplyActualChange(organization, actor, access_token, request, source,
        route_rules::ActualChange::Record, std::move(current));
}

models::Transportation ActualsComponent::CorrectActual(const boost::uuids::uuid& organization,
    const platform::auth::Identity& actor, std::string_view access_token,
    const models::CorrectActualRequest& request, const models::ShipmentContext& source,
    models::Transportation current) const {
    return ApplyActualChange(organization, actor, access_token, request, source,
        route_rules::ActualChange::Correction, std::move(current));
}
}  // namespace transportations
