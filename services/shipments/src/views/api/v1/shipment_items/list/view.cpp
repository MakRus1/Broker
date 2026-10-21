#include <helpers/rows.hpp>
#include <map>
#include <platform/auth/principal.hpp>
#include <views/api/v1/shipment_items/list/view.hpp>

namespace shipments::views::api::v1::shipment_items::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    try {
        const auto& principal = platform::auth::GetPrincipal(context);
        Session session{deps.pg_cluster, principal, std::nullopt, kReadSnapshot};
        if (session.Tx.Execute(sql::kGetShipment, session.Tenant, request.body.shipment_id)
                .IsEmpty())
            throw ApiError{404, "not_found", "Поставка не найдена"};
        const auto rows = session.Tx
                              .Execute(sql::kListItems, session.Tenant, request.body.shipment_id,
                                  request.body.after, request.body.limit)
                              .AsContainer<std::vector<pgtypes::ShipmentItemV1>>();
        std::vector<std::string> ids;
        for (const auto& row : rows)
            ids.push_back(boost::uuids::to_string(row.Id));
        std::map<std::string, pgtypes::QuantityTotalsV1> totals;
        if (!ids.empty()) {
            const auto counts = session.Tx.Execute(sql::kListOperationTotals, session.Tenant, ids);
            for (const auto& count : counts.AsSetOf<pgtypes::ItemTotalsV1>())
                totals.emplace(boost::uuids::to_string(count.ItemId),
                    pgtypes::QuantityTotalsV1{count.Created, count.Pending});
        }
        std::vector<models::ShipmentItem> result;
        for (const auto& row : rows) {
            const auto found = totals.find(boost::uuids::to_string(row.Id));
            result.push_back(ToModel(
                row, found == totals.end() ? pgtypes::QuantityTotalsV1{0, 0} : found->second));
        }
        session.Tx.Commit();
        return Response200{.items = result};
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
}  // namespace shipments::views::api::v1::shipment_items::list
