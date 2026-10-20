#include <platform/auth/principal.hpp>
#include <repositories/store.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <views/api/v1/shipments/update/view.hpp>

namespace shipments::views::api::v1::shipments::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    userver::formats::json::ValueBuilder body{userver::formats::json::Type::kObject};
    body["shipment_id"] = request.body.shipment_id;
    body["expected_revision"] = request.body.expected_revision;
    body["reference"] = request.body.reference;
    try {
        const auto result = ::shipments::Store{deps.pg_cluster}.UpdateShipment(
            platform::auth::GetPrincipal(context), body.ExtractValue());
        return Response200{.shipment = ::shipments::models::ParseModelJson(
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
}  // namespace shipments::views::api::v1::shipments::update
