#include <domain/route.hpp>
#include <helpers/rows.hpp>
#include <platform/auth/principal.hpp>
#include <views/api/v1/transportation_routes/set/view.hpp>

namespace transportations::views::api::v1::transportation_routes::set {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::transportations::helpers;
    try {
        const auto& principal = platform::auth::GetPrincipal(context);
        Session session{deps.pg_cluster, principal};
        auto result = ReadTransportation(session, request.body.transportation_id, true);
        CheckRevision(result.revision, request.body.expected_revision);
        for (const auto& leg : result.legs) {
            if (leg.actual_departure_at.has_value() || leg.actual_arrival_at.has_value())
                throw ApiError{409, "route_has_actuals", "Маршрут с фактами не заменяется"};
        }
        domain::ValidateRoute(request.body);
        session.Tx.Execute(sql::kDeleteLegs, session.Tenant, result.id);
        result.legs.clear();
        std::int64_t position = 0;
        for (const auto& input : request.body.legs) {
            auto leg = domain::MakeLeg(input);
            session.Tx.Execute(
                sql::kInsertLeg, session.Tenant, result.id, leg.id, position++, Document(leg));
            result.legs.push_back(std::move(leg));
        }
        session.Tx.Execute(sql::kBumpTransportationRevision, session.Tenant, result.id);
        ++result.revision;
        session.Audit("set_route", result.id, request.body, result);
        session.Tx.Commit();
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
