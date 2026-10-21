#include <helpers/rows.hpp>
#include <platform/auth/principal.hpp>
#include <views/api/v1/shipments/update/view.hpp>

namespace shipments::views::api::v1::shipments::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    try {
        const auto& principal = platform::auth::GetPrincipal(context);
        Session session{deps.pg_cluster, principal};
        const auto current =
            session.Tx.Execute(sql::kLockShipment, session.Tenant, request.body.shipment_id);
        if (current.IsEmpty())
            throw ApiError{404, "not_found", "Поставка не найдена"};
        CheckRevision(
            current.AsSingleRow<pgtypes::ShipmentV1>().Revision, request.body.expected_revision);
        const auto result =
            ToModel(session.Tx
                        .Execute(sql::kUpdateShipment, session.Tenant, request.body.shipment_id,
                            request.body.reference, request.body.expected_revision)
                        .AsSingleRow<pgtypes::ShipmentV1>());
        session.Audit("update_shipment", result.id, request.body, result);
        session.Tx.Commit();
        return Response200{.shipment = result};
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
