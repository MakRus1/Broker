#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <map>
#include <platform/auth/principal.hpp>
#include <views/api/v1/shipment_items/list/view.hpp>

namespace shipments::views::api::v1::shipment_items::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    try {
        const auto& principal = platform::auth::GetPrincipal(context);
        // Позиции и счётчики резервов читаются из одного снимка.
        auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, kReadSnapshot);
        const auto tenant = ResolveTenant(transaction, principal);
        if (transaction.Execute(sql::kGetShipment, tenant, request.body.shipment_id).IsEmpty())
            throw ApiError{404, "not_found", "Поставка не найдена"};
        const auto rows = transaction
                              .Execute(sql::kListItems, tenant, request.body.shipment_id,
                                  request.body.after, request.body.limit)
                              .AsContainer<std::vector<pgtypes::ShipmentItemV1>>();
        std::vector<std::string> ids;
        for (const auto& row : rows)
            ids.push_back(boost::uuids::to_string(row.Id));
        std::map<std::string, pgtypes::QuantityTotalsV1> totals;
        if (!ids.empty()) {
            const auto counts = transaction.Execute(sql::kListOperationTotals, tenant, ids);
            for (const auto& count : counts.AsSetOf<pgtypes::ItemTotalsV1>())
                totals.emplace(boost::uuids::to_string(count.ItemId),
                    pgtypes::QuantityTotalsV1{count.Created, count.Pending});
        }
        const auto result = Convert(rows, totals);
        transaction.Commit();
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
