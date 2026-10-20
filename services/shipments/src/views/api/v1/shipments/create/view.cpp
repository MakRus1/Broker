#include <platform/auth/principal.hpp>
#include <repositories/store.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <views/api/v1/shipments/create/view.hpp>

namespace shipments::views::api::v1::shipments::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    userver::formats::json::ValueBuilder body{userver::formats::json::Type::kObject};
    body["customer_id"] = request.body.customer_id;
    if (request.body.reference)
        body["reference"] = *request.body.reference;
    body["idempotency_key"] = request.body.idempotency_key;
    try {
        const auto result = ::shipments::Store{deps.pg_cluster}.CreateShipment(
            platform::auth::GetPrincipal(context), body.ExtractValue());
        return Response201{.shipment = ::shipments::models::ParseModelJson(
                               result, std::type_identity<::shipments::models::Shipment>{})};
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
}  // namespace shipments::views::api::v1::shipments::create
