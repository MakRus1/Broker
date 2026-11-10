#include <components/actuals.hpp>
#include <domain/actuals.hpp>
#include <domain/inherited_route.hpp>
#include <helpers/revision.hpp>
#include <helpers/transportations.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace transportations {
using namespace helpers;
using namespace converters;
using namespace domain;

ActualsComponent::ActualsComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-transportations")
                   .GetCluster()),
      references_(context.FindComponent<ReferencesComponent>()),
      transportations_(context.FindComponent<TransportationsComponent>()) {}

userver::yaml_config::Schema ActualsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

template <typename Request>
models::TransportationResult ActualsComponent::Apply(const Request& request,
    userver::server::request::RequestContext& context, domain::ActualChange change) const {

    platform::auth::CheckPermission(context, permissions::TRANSPORTATIONS_WRITE);
    const auto& identity = platform::auth::GetIdentity(context);
    auto loaded = transportations_.LoadLocal(context, request.transportation_id);
    CheckRevision(loaded.revision, request.expected_revision);
    const auto sources = references_.LoadShipmentContexts(context, {loaded.shipment_id});
    const auto& source = FindShipmentContext(sources, loaded.shipment_id);
    InheritRoute(loaded, source);
    ApplyActual(loaded, request.route_segment_id, request.event_kind, request.occurred_at, change);
    references_.RegisterRouteUsage(context, source, {request.route_segment_id});
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto organization = platform::auth::GetOrganization(identity);
    auto result = ReadTransportation(transaction, organization, request.transportation_id, true);
    CheckRevision(result.revision, request.expected_revision);
    InheritRoute(result, source);
    const auto position = ApplyActual(
        result, request.route_segment_id, request.event_kind, request.occurred_at, change);
    transaction.Execute(sql::kInsertRouteSegment, organization, result.id, request.route_segment_id,
        static_cast<std::int64_t>(position), Document(result.route_segments.at(position)));
    transaction.Execute(sql::kBumpTransportationRevision, organization, result.id);
    ++result.revision;
    WriteAudit(transaction, organization, identity,
        change == ActualChange::Record ? "record_actual" : "correct_actual", result.id, request,
        result);
    transaction.Commit();
    return models::TransportationResult{.transportation = result};
}

models::TransportationResult ActualsComponent::Record(
    models::RecordActualRequest request, userver::server::request::RequestContext& context) const {
    return Apply(request, context, domain::ActualChange::Record);
}

models::TransportationResult ActualsComponent::Correct(
    models::CorrectActualRequest request, userver::server::request::RequestContext& context) const {
    return Apply(request, context, domain::ActualChange::Correction);
}
}  // namespace transportations
