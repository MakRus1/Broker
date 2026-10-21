#include <platform/auth/principal.hpp>
#include <repositories/store.hpp>
#include <views/api/v1/shipment_items/update/view.hpp>

namespace shipments::views::api::v1::shipment_items::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const ::shipments::models::UpdateItemRequest command{
        .expected_revision = request.body.expected_revision,
        .item_id = request.body.item_id,
        .planned_quantity = request.body.planned_quantity};
    try {
        return Response200{.item = ::shipments::Store{deps.pg_cluster}.UpdateItem(
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
}  // namespace shipments::views::api::v1::shipment_items::update
