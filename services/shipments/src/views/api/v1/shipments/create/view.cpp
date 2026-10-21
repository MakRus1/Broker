#include <helpers/rows.hpp>
#include <platform/auth/principal.hpp>
#include <views/api/v1/shipments/create/view.hpp>

namespace shipments::views::api::v1::shipments::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    try {
        const auto& principal = platform::auth::GetPrincipal(context);
        Session session{deps.pg_cluster, principal};
        const auto replay = FindReplay<models::Shipment>(session, "create_shipment", request.body);
        if (replay.has_value()) {
            session.Tx.Commit();
            return Response201{.shipment = replay.value()};
        }
        const auto row = session.Tx
                             .Execute(sql::kCreateShipment, session.Tenant,
                                 request.body.customer_id, request.body.reference.value_or(""))
                             .AsSingleRow<ShipmentRow>(pg::kRowTag);
        const auto result = ToModel(row);
        SaveReplay(session, "create_shipment", request.body, result);
        session.Audit("create_shipment", result.id, request.body, result);
        session.Tx.Commit();
        return Response201{.shipment = result};
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
