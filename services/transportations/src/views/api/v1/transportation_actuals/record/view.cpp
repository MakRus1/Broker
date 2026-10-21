#include <helpers/actuals.hpp>
#include <helpers/rows.hpp>
#include <platform/auth/principal.hpp>
#include <views/api/v1/transportation_actuals/record/view.hpp>

namespace transportations::views::api::v1::transportation_actuals::record {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::transportations::helpers;
    try {
        const auto& principal = platform::auth::GetPrincipal(context);
        Session session{deps.pg_cluster, principal};
        auto result = ReadTransportation(session, request.body.transportation_id, true);
        CheckRevision(result.revision, request.body.expected_revision);
        UpdateActual(session, result, request.body.leg_id, request.body.event_kind,
            request.body.occurred_at, ActualChange::Record);
        session.Audit("record_actual", result.id, request.body, result);
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
}  // namespace transportations::views::api::v1::transportation_actuals::record
