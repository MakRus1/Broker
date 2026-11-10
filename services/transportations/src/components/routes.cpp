#include <components/routes.hpp>
#include <converters/route.hpp>
#include <converters/transportations.hpp>
#include <domain/inherited_route.hpp>
#include <domain/rail.hpp>
#include <domain/route.hpp>
#include <helpers/database.hpp>
#include <helpers/revision.hpp>
#include <helpers/transportations.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <set>
#include <userver/components/component_context.hpp>
#include <userver/formats/json.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/text.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace transportations {
using namespace helpers;
using namespace converters;
using namespace domain;

RoutesComponent::RoutesComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-transportations")
                   .GetCluster()),
      references_(context.FindComponent<ReferencesComponent>()),
      transportations_(context.FindComponent<TransportationsComponent>()) {}

userver::yaml_config::Schema RoutesComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::RouteUsageResult RoutesComponent::Usage(
    models::RouteUsageRequest request, userver::server::request::RequestContext& context) const {

    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    const auto rows = cluster_
                          ->Execute(pg::ClusterHostType::kMaster, sql::kRouteUsage, organization,
                              request.shipment_id)
                          .AsContainer<std::vector<Json>>();
    std::set<std::string> ids;
    for (const auto& segment : Convert(rows))
        if (HasRouteData(segment))
            ids.insert(segment.id);
    return models::RouteUsageResult{.segment_ids = {ids.begin(), ids.end()}};
}

models::RouteProjectionResult RoutesComponent::Project(models::ShipmentRouteProjection request,
    userver::server::request::RequestContext& context) const {
    platform::auth::CheckPermission(context, permissions::TRANSPORTATIONS_CREATE);
    const auto& body = request;
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto advanced = transaction.Execute(
        sql::kAdvanceRouteProjection, body.organization_id, body.shipment_id, body.revision);
    if (advanced.IsEmpty()) {
        transaction.Commit();
        return models::RouteProjectionResult{.applied = false};
    }
    transaction.Execute(sql::kClearRouteProjection, body.organization_id, body.shipment_id);
    for (const auto& segment : body.route_segments)
        transaction.Execute(sql::kSaveRouteProjectionSegment, body.organization_id,
            body.shipment_id, segment.id, segment.type,
            userver::formats::json::ValueBuilder(segment).ExtractValue());
    transaction.Commit();
    return models::RouteProjectionResult{.applied = true};
}

models::TransportationResult RoutesComponent::Set(
    models::SetRouteRequest request, userver::server::request::RequestContext& context) const {

    platform::auth::CheckPermission(context, permissions::TRANSPORTATIONS_WRITE);
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    auto loaded = transportations_.LoadLocal(context, request.transportation_id);
    CheckRevision(loaded.revision, request.expected_revision);
    const auto sources = references_.LoadShipmentContexts(context, {loaded.shipment_id});
    const auto& source = FindShipmentContext(sources, loaded.shipment_id);
    InheritRoute(loaded, source);
    domain::ValidateRoute(request);
    references_.ValidateRouteCounterparties(context, request.route_segments, loaded.route_segments);
    const auto stations =
        references_.LoadRailwayStations(context, domain::GetRailwayStationIds(request));
    domain::ResolveRailwayStations(request, loaded, stations);
    if (request.route_segments.size() != source.route_segments.size())
        throw ApiError<409>{"inherited_route", "Состав маршрута изменяется только в поставке"};
    std::vector<std::string> used;
    for (std::size_t i = 0; i < source.route_segments.size(); ++i) {
        const auto& input = request.route_segments[i];
        const auto& stage = source.route_segments[i];
        if (input.id != stage.id || models::EnumValue(input.mode) != stage.type)
            throw ApiError<409>{
                "inherited_route", "Порядок и типы участков изменяются только в поставке"};
        if (HasRouteData(converters::Convert(input)))
            used.push_back(input.id);
    }
    references_.RegisterRouteUsage(context, source, used);
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    auto result = ReadTransportation(transaction, organization, loaded.id, true);
    CheckRevision(result.revision, request.expected_revision);
    InheritRoute(result, source);
    for (std::size_t i = 0; i < request.route_segments.size(); ++i) {
        auto& stored = result.route_segments[i];
        auto value = converters::Convert(request.route_segments[i]);
        value.inherited = stored.inherited;
        value.sea = stored.sea;
        value.air = stored.air;
        value.actual_departure_at = stored.actual_departure_at;
        value.actual_arrival_at = stored.actual_arrival_at;
        domain::ValidateActualTimes(value);
        transaction.Execute(sql::kInsertRouteSegment, organization, result.id, value.id,
            static_cast<std::int64_t>(i), Document(value));
        stored = std::move(value);
    }
    transaction.Execute(sql::kBumpTransportationRevision, organization, result.id);
    ++result.revision;
    WriteAudit(transaction, organization, identity, "set_route", result.id, request, result);
    transaction.Commit();
    return models::TransportationResult{.transportation = result};
}
}  // namespace transportations
