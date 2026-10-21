#include <domain/route.hpp>
#include <helpers/revision.hpp>
#include <helpers/transportations.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/transportation_routes/set/view.hpp>

namespace transportations::views::api::v1::transportation_routes::set {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::transportations::helpers;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto organization = platform::auth::GetOrganization(identity);
        auto result =
            ReadTransportation(transaction, organization, request.body.transportation_id, true);
        CheckRevision(result.revision, request.body.expected_revision);
        for (const auto& route_segment : result.route_segments) {
            if (route_segment.actual_departure_at.has_value() ||
                route_segment.actual_arrival_at.has_value())
                throw ApiError{409, "route_has_actuals", "Маршрут с фактами не заменяется"};
        }
        domain::ValidateRoute(request.body);
        transaction.Execute(sql::kDeleteRouteSegments, organization, result.id);
        result.route_segments.clear();
        std::int64_t position = 0;
        for (const auto& input : request.body.route_segments) {
            auto route_segment = domain::MakeRouteSegment(input);
            transaction.Execute(sql::kInsertRouteSegment, organization, result.id, route_segment.id,
                position++, Document(route_segment));
            result.route_segments.push_back(std::move(route_segment));
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
        default:
            return Response409{.error = error.Code, .message = error.what()};
        }
    }
}
}  // namespace transportations::views::api::v1::transportation_routes::set
