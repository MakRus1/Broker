#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/shipments/get/view.hpp>

namespace shipments::views::api::v1::shipments::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::SHIPMENTS_READ);
        const auto organization = platform::auth::GetOrganization(identity);
        const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kGetShipment,
            organization, request.body.shipment_id);
        if (rows.IsEmpty())
            throw ApiError{404, "not_found", "Поставка не найдена"};
        const auto result = Convert(rows.AsSingleRow<pgtypes::ShipmentV3>());
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
