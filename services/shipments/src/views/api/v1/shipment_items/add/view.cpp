#include <platform/auth/principal.hpp>
#include <repositories/store.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <views/api/v1/shipment_items/add/view.hpp>

namespace shipments::views::api::v1::shipment_items::add {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    userver::formats::json::ValueBuilder body{userver::formats::json::Type::kObject};
    body["shipment_id"] = request.body.shipment_id;
    body["expected_revision"] = request.body.expected_revision;
    body["container_type_code"] = request.body.container_type_code;
    body["container_ownership"] = request.body.container_ownership;
    body["planned_quantity"] = request.body.planned_quantity;
    body["idempotency_key"] = request.body.idempotency_key;
    try {
        const auto result = ::shipments::Store{deps.pg_cluster}.AddItem(
            platform::auth::GetPrincipal(context), body.ExtractValue());
        return Response201{.item = ::shipments::models::ParseModelJson(
                               result, std::type_identity<::shipments::models::ShipmentItem>{})};
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
