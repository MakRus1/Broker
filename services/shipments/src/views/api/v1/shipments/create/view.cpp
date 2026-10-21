#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <helpers/idempotency.hpp>
#include <platform/auth/principal.hpp>
#include <views/api/v1/shipments/create/view.hpp>

namespace shipments::views::api::v1::shipments::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    try {
        const auto& principal = platform::auth::GetPrincipal(context);
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto tenant = ResolveTenant(transaction, principal);
        const auto replay =
            FindReplay<models::Shipment>(transaction, tenant, "create_shipment", request.body);
        if (replay.has_value()) {
            transaction.Commit();
            return Response201{.shipment = replay.value()};
        }
        const auto row = transaction
                             .Execute(sql::kCreateShipment, tenant, request.body.customer_id,
                                 request.body.reference.value_or(""))
                             .AsSingleRow<pgtypes::ShipmentV1>();
        const auto result = Convert(row);
        SaveReplay(transaction, tenant, "create_shipment", request.body, result);
        WriteAudit(
            transaction, tenant, principal, "create_shipment", result.id, request.body, result);
        transaction.Commit();
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
