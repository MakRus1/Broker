#include <components/inherited_route.hpp>
#include <components/rail_route.hpp>
#include <components/route_integrations.hpp>
#include <components/route_validation.hpp>
#include <components/routes.hpp>
#include <converters/route.hpp>
#include <converters/transportations.hpp>
#include <helpers/database.hpp>
#include <helpers/revision.hpp>
#include <helpers/route_usage.hpp>
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
    transaction.Execute(sql::kLockShipmentRouteUsage, organization, shipment_id);
    transaction.Execute(sql::kAdvanceUsageCheckVersion, organization, shipment_id);
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
    bool applied = false;
    if (body.customer_id.has_value()) {
        const auto updated = transaction.Execute(sql::kUpsertShipmentCustomerProjection,
            body.organization_id, body.shipment_id, body.revision, body.customer_id.value());
        applied = !updated.IsEmpty();
    }
    if (body.route_segments.has_value()) {
        const auto advanced = transaction.Execute(
            sql::kAdvanceRouteProjection, body.organization_id, body.shipment_id, body.revision);
        if (!advanced.IsEmpty()) {
            transaction.Execute(sql::kClearRouteProjection, body.organization_id, body.shipment_id);
            for (const auto& segment : body.route_segments.value())
                transaction.Execute(sql::kSaveRouteProjectionSegment, body.organization_id,
                    body.shipment_id, segment.id, segment.type,
                    userver::formats::json::ValueBuilder(segment).ExtractValue());
            applied = true;
        }
    }
    transaction.Commit();
    return models::RouteProjectionResult{.applied = applied};
}

models::Transportation RoutesComponent::SaveRoute(const boost::uuids::uuid& organization,
    const platform::auth::Identity& identity, std::string_view access_token,
    models::SetRouteRequest& request, const models::ShipmentContext& source,
    models::Transportation result) const {
    const auto usage_check_version =
        PrepareRouteWrite(organization, access_token, request, source, result);
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    helpers::CheckRouteWrite(transaction, organization, source.shipment_id, usage_check_version);
    const auto updated = transaction.Execute(sql::kUpdateRouteSegments, organization, result.id,
        request.expected_revision, converters::ToStoredRoute(result.route_segments));
    if (updated.IsEmpty())
        throw RevisionConflict{};
    result.revision = updated.AsSingleRow<std::int64_t>();
    helpers::WriteAudit(
        transaction, organization, identity, "set_route", result.id, request, result);
    transaction.Commit();
    return result;
}

std::optional<std::int64_t> RoutesComponent::PrepareRouteWrite(
    const boost::uuids::uuid& organization, std::string_view access_token,
    models::SetRouteRequest& request, const models::ShipmentContext& source,
    models::Transportation& result) const {
    route_rules::InheritRoute(result, source);
    route_rules::ValidateRoute(request);
    integrations_.PrepareRoute(access_token, request, result);
    const auto& segments = request.route_segments;
    if (segments.size() != source.route_segments.size())
        throw ApiError<409>{"inherited_route", "Состав маршрута изменяется только в поставке"};
    for (std::size_t i = 0; i < source.route_segments.size(); ++i) {
        const auto& input = segments[i];
        const auto& stage = source.route_segments[i];
        if (input.id != stage.id || models::EnumValue(input.mode) != stage.type)
            throw ApiError<409>{
                "inherited_route", "Порядок и типы участков изменяются только в поставке"};
    }

    std::vector<boost::uuids::uuid> used;
    for (std::size_t i = 0; i < segments.size(); ++i) {
        auto& stored = result.route_segments[i];
        const auto& input = segments[i];
        auto value = converters::Convert(input);
        value.sea_unloading_date = input.sea_unloading_date.has_value()
            ? input.sea_unloading_date.value()
            : stored.sea_unloading_date;
        if (route_rules::HasRouteData(value))
            used.push_back(value.id);

        value.inherited = stored.inherited;
        value.plan = stored.plan;
        value.sea = stored.sea;
        value.air = stored.air;
        value.actual_departure_at = stored.actual_departure_at;
        value.actual_arrival_at = stored.actual_arrival_at;
        route_rules::ValidateActualTimes(value);
        stored = std::move(value);
    }

    return RegisterUse(organization, access_token, source, used);
}

std::optional<std::int64_t> RoutesComponent::RegisterUse(const boost::uuids::uuid& organization,
    std::string_view access_token, const models::ShipmentContext& source,
    const std::vector<boost::uuids::uuid>& used) const {
    if (used.empty())
        return std::nullopt;
    const auto usage_check_version =
        cluster_
            ->Execute(pg::ClusterHostType::kMaster, sql::kGetUsageCheckVersion, organization,
                source.shipment_id)
            .AsSingleRow<std::int64_t>();
    integrations_.RegisterRouteUsage(access_token, source, used);
    return usage_check_version;
}
}  // namespace transportations
