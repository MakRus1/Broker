#include <components/inherited_route.hpp>
#include <components/rail_route.hpp>
#include <components/route_integrations.hpp>
#include <components/route_validation.hpp>
#include <components/routes.hpp>
#include <converters/route.hpp>
#include <converters/transportations.hpp>
#include <helpers/database.hpp>
#include <helpers/revision.hpp>
#include <helpers/transportations.hpp>
#include <platform/auth/identity.hpp>
#include <set>
#include <userver/components/component_context.hpp>
#include <userver/formats/json.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/array_types.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>
#include <userver/utils/text.hpp>

namespace transportations {
namespace pg = userver::storages::postgres;
namespace json = userver::formats::json;

RoutesComponent::RoutesComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-transportations")
                   .GetCluster()),
      integrations_(context.FindComponent<RouteIntegrations>()) {}

models::RouteUsageResult RoutesComponent::FindRouteUsage(
    const boost::uuids::uuid& organization, const boost::uuids::uuid& shipment_id) const {

    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    // Отдельное чтение после блокировки видит зафиксированные за время ожидания изменения.
    transaction.Execute(sql::kLockShipmentTransportations, organization, shipment_id);
    const auto rows = transaction.Execute(sql::kRouteUsage, organization, shipment_id)
                          .AsContainer<std::vector<json::Value>>();
    transaction.Commit();

    std::set<boost::uuids::uuid> ids;
    for (const auto& segment : converters::Convert(rows))
        if (route_rules::HasRouteData(segment))
            ids.insert(segment.id);
    return models::RouteUsageResult{.segment_ids = {ids.begin(), ids.end()}};
}

models::RouteProjectionResult RoutesComponent::ApplyShipmentRouteProjection(
    const models::ShipmentRouteProjection& request) const {
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

models::Transportation RoutesComponent::SaveRoute(const boost::uuids::uuid& organization,
    const platform::auth::Identity& identity, std::string_view access_token,
    const models::SetRouteRequest& request, const models::ShipmentContext& source) const {
    if (request.route_segments.size() != source.route_segments.size())
        throw ApiError<409>{"inherited_route", "Состав маршрута изменяется только в поставке"};
    for (std::size_t i = 0; i < source.route_segments.size(); ++i) {
        const auto& input = request.route_segments[i];
        const auto& stage = source.route_segments[i];
        if (input.id != stage.id || models::EnumValue(input.mode) != stage.type)
            throw ApiError<409>{
                "inherited_route", "Порядок и типы участков изменяются только в поставке"};
    }

    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});

    auto result = helpers::ReadTransportation(
        transaction, organization, request.transportation_id, helpers::ReadMode::ForUpdate);
    helpers::CheckRevision(result.revision, request.expected_revision);
    route_rules::InheritRoute(result, source);

    std::vector<boost::uuids::uuid> used;
    std::vector<boost::uuids::uuid> segment_ids;
    std::vector<json::Value> documents;
    segment_ids.reserve(request.route_segments.size());
    documents.reserve(request.route_segments.size());
    for (std::size_t i = 0; i < request.route_segments.size(); ++i) {
        auto& stored = result.route_segments[i];
        auto value = converters::Convert(request.route_segments[i]);
        if (route_rules::HasRouteData(value))
            used.push_back(value.id);

        value.inherited = stored.inherited;
        value.plan = stored.plan;
        value.sea = stored.sea;
        value.air = stored.air;
        value.actual_departure_at = stored.actual_departure_at;
        value.actual_arrival_at = stored.actual_arrival_at;
        route_rules::ValidateActualTimes(value);
        segment_ids.push_back(value.id);
        documents.push_back(converters::ToStoredRouteSegment(value));
        stored = std::move(value);
    }

    integrations_.RegisterRouteUsage(access_token, source, used);
    if (!segment_ids.empty())
        transaction.Execute(
            sql::kSaveRouteSegments, organization, result.id, segment_ids, documents);

    transaction.Execute(sql::kBumpTransportationRevision, organization, result.id);
    ++result.revision;

    helpers::WriteAudit(
        transaction, organization, identity, "set_route", result.id, request, result);
    transaction.Commit();
    return result;
}
}  // namespace transportations
