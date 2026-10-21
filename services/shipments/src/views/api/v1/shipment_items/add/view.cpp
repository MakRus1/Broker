#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <helpers/idempotency.hpp>
#include <helpers/revision.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/shipment_items/add/view.hpp>

namespace shipments::views::api::v1::shipment_items::add {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto organization = ResolveOrganization(transaction, identity);
        const auto replay =
            FindReplay<models::ShipmentItem>(transaction, organization, "add_item", request.body);
        if (replay.has_value()) {
            transaction.Commit();
            return Response201{.item = replay.value()};
        }
        const auto shipment =
            transaction.Execute(sql::kLockShipment, organization, request.body.shipment_id);
        if (shipment.IsEmpty())
            throw ApiError{404, "not_found", "Поставка не найдена"};
        CheckRevision(
            shipment.AsSingleRow<pgtypes::ShipmentV1>().Revision, request.body.expected_revision);
        const auto row = transaction
                             .Execute(sql::kAddItem, organization, request.body.shipment_id,
                                 request.body.container_type_code,
                                 models::EnumValue(request.body.container_ownership),
                                 request.body.planned_quantity)
                             .AsSingleRow<pgtypes::ShipmentItemV1>();
        transaction.Execute(sql::kBumpShipmentRevision, organization, request.body.shipment_id);
        const auto result = Convert(row, pgtypes::QuantityTotalsV1{0, 0});
        SaveReplay(transaction, organization, "add_item", request.body, result);
        WriteAudit(
            transaction, organization, identity, "add_item", result.id, request.body, result);
        transaction.Commit();
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
