#include <platform/auth/principal.hpp>
#include <repositories/store.hpp>
#include <views/api/v1/shipment_items/add/view.hpp>

namespace shipments::views::api::v1::shipment_items::add {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const ::shipments::models::AddItemRequest command{
        .container_ownership = request.body.container_ownership,
        .container_type_code = request.body.container_type_code,
        .expected_revision = request.body.expected_revision,
        .idempotency_key = request.body.idempotency_key,
        .planned_quantity = request.body.planned_quantity,
        .shipment_id = request.body.shipment_id};
    try {
        return Response201{.item = ::shipments::Store{deps.pg_cluster}.AddItem(
                               platform::auth::GetPrincipal(context), command)};
    } catch (const ::shipments::ApiError& error) {
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
}  // namespace shipments::views::api::v1::shipment_items::add
