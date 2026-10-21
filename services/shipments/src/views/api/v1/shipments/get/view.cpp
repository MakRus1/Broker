#include <helpers/rows.hpp>
#include <platform/auth/principal.hpp>
#include <views/api/v1/shipments/get/view.hpp>

namespace shipments::views::api::v1::shipments::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    try {
        const auto& principal = platform::auth::GetPrincipal(context);
        Session session{deps.pg_cluster, principal, std::nullopt, kReadSnapshot};
        const auto rows =
            session.Tx.Execute(sql::kGetShipment, session.Tenant, request.body.shipment_id);
        if (rows.IsEmpty())
            throw ApiError{404, "not_found", "Поставка не найдена"};
        const auto result = ToModel(rows.AsSingleRow<pgtypes::ShipmentV1>());
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
}  // namespace shipments::views::api::v1::shipments::get
