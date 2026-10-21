#include <helpers/rows.hpp>
#include <platform/auth/principal.hpp>
#include <views/api/v1/shipment_items/update/view.hpp>

namespace shipments::views::api::v1::shipment_items::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    try {
        const auto& principal = platform::auth::GetPrincipal(context);
        Session session{deps.pg_cluster, principal};
        const auto rows = session.Tx.Execute(sql::kLockItem, session.Tenant, request.body.item_id);
        if (rows.IsEmpty())
            throw ApiError{404, "not_found", "Позиция не найдена"};
        CheckRevision(
            rows.AsSingleRow<ItemRow>(pg::kRowTag).Revision, request.body.expected_revision);
        const auto totals = ReadTotals(session, request.body.item_id);
        if (request.body.planned_quantity < totals.Created + totals.Pending)
            throw ApiError{409, "quantity_reserved", "Количество уже зарезервировано или создано"};
        const auto row = session.Tx
                             .Execute(sql::kUpdateItem, session.Tenant, request.body.item_id,
                                 request.body.planned_quantity)
                             .AsSingleRow<ItemRow>(pg::kRowTag);
        const auto result = ToModel(row, totals);
        session.Audit("update_item", result.id, request.body, result);
        session.Tx.Commit();
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
