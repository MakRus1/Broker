#include <components/actual_changes.hpp>
#include <components/actuals.hpp>
#include <components/inherited_route.hpp>
#include <components/route_integrations.hpp>
#include <converters/route.hpp>
#include <helpers/revision.hpp>
#include <helpers/transportations.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>

namespace transportations {
namespace pg = userver::storages::postgres;
namespace json = userver::formats::json;

ActualsComponent::ActualsComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-transportations")
                   .GetCluster()),
      integrations_(context.FindComponent<RouteIntegrations>()) {}

template <typename Request>
models::Transportation ActualsComponent::ApplyActualChange(const boost::uuids::uuid& organization,
    const platform::auth::Identity& identity, std::string_view access_token, const Request& request,
    const models::ShipmentContext& source, route_rules::ActualChange change) const {

    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});

    auto result = helpers::ReadTransportation(
        transaction, organization, request.transportation_id, helpers::ReadMode::ForUpdate);
    helpers::CheckRevision(result.revision, request.expected_revision);
    route_rules::InheritRoute(result, source);
    route_rules::ApplyActual(
        result, request.route_segment_id, request.event_kind, request.occurred_at, change);

    integrations_.RegisterRouteUsage(access_token, source, {request.route_segment_id});

    transaction.Execute(sql::kSaveRouteSegments, organization, result.id,
        converters::ToStoredRoute(result.route_segments));

    transaction.Execute(sql::kBumpTransportationRevision, organization, result.id);
    ++result.revision;

    helpers::WriteAudit(transaction, organization, identity,
        change == route_rules::ActualChange::Record ? "record_actual" : "correct_actual", result.id,
        request, result);
    transaction.Commit();
    return result;
}

models::Transportation ActualsComponent::RecordActual(const boost::uuids::uuid& organization,
    const platform::auth::Identity& actor, std::string_view access_token,
    const models::RecordActualRequest& request, const models::ShipmentContext& source) const {
    return ApplyActualChange(
        organization, actor, access_token, request, source, route_rules::ActualChange::Record);
}

models::Transportation ActualsComponent::CorrectActual(const boost::uuids::uuid& organization,
    const platform::auth::Identity& actor, std::string_view access_token,
    const models::CorrectActualRequest& request, const models::ShipmentContext& source) const {
    return ApplyActualChange(
        organization, actor, access_token, request, source, route_rules::ActualChange::Correction);
}
}  // namespace transportations
