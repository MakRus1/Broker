#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <helpers/idempotency.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/shipments/create/view.hpp>

namespace shipments::views::api::v1::shipments::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto organization = platform::auth::GetOrganization(identity);
        const auto replay = FindReplay<models::Shipment>(
            transaction, organization, "create_shipment", request.body);
        if (replay.has_value()) {
            transaction.Commit();
            return Response201{.shipment = replay.value()};
        }
        const auto row = transaction
                             .Execute(sql::kCreateShipment, organization, request.body.customer_id,
                                 request.body.reference.value_or(""))
                             .AsSingleRow<pgtypes::ShipmentV1>();
        const auto result = Convert(row);
        SaveReplay(transaction, organization, "create_shipment", request.body, result);
        WriteAudit(transaction, organization, identity, "create_shipment", result.id, request.body,
            result);
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
