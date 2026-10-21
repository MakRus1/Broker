#include <platform/auth/principal.hpp>
#include <repositories/store.hpp>
#include <views/api/v1/shipments/get/view.hpp>

namespace shipments::views::api::v1::shipments::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const ::shipments::models::GetShipmentRequest command{.shipment_id = request.body.shipment_id};
    try {
        return Response200{.shipment = ::shipments::Store{deps.pg_cluster}.GetShipment(
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
}  // namespace shipments::views::api::v1::shipments::get
