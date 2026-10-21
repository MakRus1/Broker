#include <platform/auth/principal.hpp>
#include <repositories/store.hpp>
#include <views/api/v1/transportations/list/view.hpp>

namespace transportations::views::api::v1::transportations::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const ::transportations::models::ListTransportationsRequest command{.after = request.body.after,
        .item_id = request.body.item_id,
        .limit = request.body.limit,
        .shipment_id = request.body.shipment_id};
    try {
        return Response200{.items = ::transportations::Store{deps.pg_cluster}.ListTransportations(
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
}  // namespace transportations::views::api::v1::transportations::list
