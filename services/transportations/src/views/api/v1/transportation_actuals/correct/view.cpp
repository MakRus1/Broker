#include <platform/auth/principal.hpp>
#include <repositories/store.hpp>
#include <views/api/v1/transportation_actuals/correct/view.hpp>

namespace transportations::views::api::v1::transportation_actuals::correct {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const ::transportations::models::CorrectActualRequest command{
        .event_kind = request.body.event_kind,
        .expected_revision = request.body.expected_revision,
        .leg_id = request.body.leg_id,
        .occurred_at = request.body.occurred_at,
        .reason = request.body.reason,
        .transportation_id = request.body.transportation_id};
    try {
        return Response200{
            .transportation = ::transportations::Store{deps.pg_cluster}.CorrectActual(
                platform::auth::GetPrincipal(context), command)};
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
}  // namespace transportations::views::api::v1::transportation_actuals::correct
