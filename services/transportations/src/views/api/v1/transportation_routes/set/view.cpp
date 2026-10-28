#include <domain/rail.hpp>
#include <domain/route.hpp>
#include <helpers/inherited_route.hpp>
#include <helpers/railway_stations.hpp>
#include <helpers/revision.hpp>
#include <helpers/route_counterparties.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <userver/utils/text.hpp>
#include <views/api/v1/transportation_routes/set/view.hpp>

namespace transportations::views::api::v1::transportation_routes::set {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::transportations::helpers;
    try {
        platform::auth::CheckPermission(context, permissions::TRANSPORTATIONS_WRITE);
        const auto& identity = platform::auth::GetIdentity(context);
        const auto organization = platform::auth::GetOrganization(identity);
        auto loaded = LoadLocalTransportation(deps, context, request.body.transportation_id);
        CheckRevision(loaded.revision, request.body.expected_revision);
        const auto sources = LoadShipmentContexts(deps, context, {loaded.shipment_id});
        const auto& source = FindShipmentContext(sources, loaded.shipment_id);
        InheritRoute(loaded, source);
        domain::ValidateRoute(request.body);
        ValidateRouteCounterparties(
            deps, context, request.body.route_segments, loaded.route_segments);
        const auto stations =
            LoadRailwayStations(deps, context, domain::GetRailwayStationIds(request.body));
        domain::ResolveRailwayStations(request.body, loaded, stations);
        if (request.body.route_segments.size() != source.route_segments.size())
            throw ApiError{409, "inherited_route", "Состав маршрута изменяется только в поставке"};
        std::vector<std::string> used;
        for (std::size_t i = 0; i < source.route_segments.size(); ++i) {
            const auto& input = request.body.route_segments[i];
            const auto& stage = source.route_segments[i];
            if (input.id != stage.id || models::EnumValue(input.mode) != stage.type)
                throw ApiError{
                    409, "inherited_route", "Порядок и типы участков изменяются только в поставке"};
            if (HasRouteData(domain::MakeRouteSegment(input)))
                used.push_back(input.id);
        }
        RegisterRouteUsage(deps, context, source, used);
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        auto result = ReadTransportation(transaction, organization, loaded.id, true);
        CheckRevision(result.revision, request.body.expected_revision);
        InheritRoute(result, source);
        for (std::size_t i = 0; i < request.body.route_segments.size(); ++i) {
            auto& stored = result.route_segments[i];
            auto value = domain::MakeRouteSegment(request.body.route_segments[i]);
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
        WriteAudit(
            transaction, organization, identity, "set_route", result.id, request.body, result);
        transaction.Commit();
        return Response200{.transportation = result};
    } catch (const ::transportations::ApiError& error) {
        switch (error.Status) {
        case 400:
            return Response400{.error = error.Code, .message = error.what()};
        case 403:
            return Response403{.error = error.Code, .message = error.what()};
        case 404:
            return Response404{.error = error.Code, .message = error.what()};
        case 503:
            return Response503{.error = error.Code, .message = error.what()};
        default:
            return Response409{.error = error.Code, .message = error.what()};
        }
    }
}
}  // namespace transportations::views::api::v1::transportation_routes::set
