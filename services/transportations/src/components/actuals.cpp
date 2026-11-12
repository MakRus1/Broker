#include <components/actual_changes.hpp>
#include <components/actuals.hpp>
#include <components/inherited_route.hpp>
#include <helpers/revision.hpp>
#include <helpers/transportations.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace transportations {
namespace pg = userver::storages::postgres;
namespace json = userver::formats::json;

ActualsComponent::ActualsComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-transportations")
                   .GetCluster()),
      integrations_(context.FindComponent<RouteIntegrations>()) {}

userver::yaml_config::Schema ActualsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

template <typename Request>
models::Transportation ActualsComponent::ApplyActualChange(const Request& request,
    const std::string& organization, const platform::auth::Identity& identity,
    std::string_view access_token, const models::ShipmentContext& source,
    route_rules::ActualChange change) const {

    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});

    auto result =
        helpers::ReadTransportation(transaction, organization, request.transportation_id, true);
    helpers::CheckRevision(result.revision, request.expected_revision);
    route_rules::InheritRoute(result, source);
    const auto position = route_rules::ApplyActual(
        result, request.route_segment_id, request.event_kind, request.occurred_at, change);

    integrations_.RegisterRouteUsage(access_token, source, {request.route_segment_id});

    transaction.Execute(sql::kInsertRouteSegment, organization, result.id, request.route_segment_id,
        static_cast<std::int64_t>(position), helpers::Document(result.route_segments.at(position)));

    transaction.Execute(sql::kBumpTransportationRevision, organization, result.id);
    ++result.revision;

    helpers::WriteAudit(transaction, organization, identity,
        change == route_rules::ActualChange::Record ? "record_actual" : "correct_actual", result.id,
        request, result);
    transaction.Commit();
    return result;
}

models::Transportation ActualsComponent::RecordActual(const std::string& organization,
    const platform::auth::Identity& actor, std::string_view access_token,
    const models::RecordActualRequest& request, const models::ShipmentContext& source) const {
    return ApplyActualChange(
        request, organization, actor, access_token, source, route_rules::ActualChange::Record);
}

models::Transportation ActualsComponent::CorrectActual(const std::string& organization,
    const platform::auth::Identity& actor, std::string_view access_token,
    const models::CorrectActualRequest& request, const models::ShipmentContext& source) const {
    return ApplyActualChange(
        request, organization, actor, access_token, source, route_rules::ActualChange::Correction);
}
}  // namespace transportations
