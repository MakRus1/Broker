#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <helpers/revision.hpp>
#include <platform/auth/principal.hpp>
#include <views/api/v1/shipments/update/view.hpp>

namespace shipments::views::api::v1::shipments::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    try {
        const auto& principal = platform::auth::GetPrincipal(context);
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto tenant = ResolveTenant(transaction, principal);
        const auto current =
            transaction.Execute(sql::kLockShipment, tenant, request.body.shipment_id);
        if (current.IsEmpty())
            throw ApiError{404, "not_found", "Поставка не найдена"};
        CheckRevision(
            current.AsSingleRow<pgtypes::ShipmentV1>().Revision, request.body.expected_revision);
        const auto result =
            Convert(transaction
                        .Execute(sql::kUpdateShipment, tenant, request.body.shipment_id,
                            request.body.reference, request.body.expected_revision)
                        .AsSingleRow<pgtypes::ShipmentV1>());
        WriteAudit(
            transaction, tenant, principal, "update_shipment", result.id, request.body, result);
        transaction.Commit();
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
