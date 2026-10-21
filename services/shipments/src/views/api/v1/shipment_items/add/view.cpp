#include <helpers/rows.hpp>
#include <platform/auth/principal.hpp>
#include <views/api/v1/shipment_items/add/view.hpp>

namespace shipments::views::api::v1::shipment_items::add {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    try {
        const auto& principal = platform::auth::GetPrincipal(context);
        Session session{deps.pg_cluster, principal};
        const auto replay = FindReplay<models::ShipmentItem>(session, "add_item", request.body);
        if (replay.has_value()) {
            session.Tx.Commit();
            return Response201{.item = replay.value()};
        }
        const auto shipment =
            session.Tx.Execute(sql::kLockShipment, session.Tenant, request.body.shipment_id);
        if (shipment.IsEmpty())
            throw ApiError{404, "not_found", "Поставка не найдена"};
        CheckRevision(
            shipment.AsSingleRow<pgtypes::ShipmentV1>().Revision, request.body.expected_revision);
        const auto row = session.Tx
                             .Execute(sql::kAddItem, session.Tenant, request.body.shipment_id,
                                 request.body.container_type_code,
                                 models::EnumValue(request.body.container_ownership),
                                 request.body.planned_quantity)
                             .AsSingleRow<pgtypes::ShipmentItemV1>();
        session.Tx.Execute(sql::kBumpShipmentRevision, session.Tenant, request.body.shipment_id);
        const auto result = ToModel(row, pgtypes::QuantityTotalsV1{0, 0});
        SaveReplay(session, "add_item", request.body, result);
        session.Audit("add_item", result.id, request.body, result);
        session.Tx.Commit();
        return Response201{.item = result};
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
