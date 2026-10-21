#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <helpers/quantities.hpp>
#include <helpers/revision.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/shipment_items/update/view.hpp>

namespace shipments::views::api::v1::shipment_items::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto organization = ResolveOrganization(transaction, identity);
        const auto rows = transaction.Execute(sql::kLockItem, organization, request.body.item_id);
        if (rows.IsEmpty())
            throw ApiError{404, "not_found", "Позиция не найдена"};
        CheckRevision(
            rows.AsSingleRow<pgtypes::ShipmentItemV1>().Revision, request.body.expected_revision);
        const auto totals = ReadTotals(transaction, organization, request.body.item_id);
        if (request.body.planned_quantity < totals.Created + totals.Pending)
            throw ApiError{409, "quantity_reserved", "Количество уже зарезервировано или создано"};
        const auto row = transaction
                             .Execute(sql::kUpdateItem, organization, request.body.item_id,
                                 request.body.planned_quantity)
                             .AsSingleRow<pgtypes::ShipmentItemV1>();
        const auto result = Convert(row, totals);
        WriteAudit(
            transaction, organization, identity, "update_item", result.id, request.body, result);
        transaction.Commit();
        return Response200{.item = result};
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
}  // namespace shipments::views::api::v1::shipment_items::update
