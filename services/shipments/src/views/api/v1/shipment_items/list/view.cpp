#include <boost/uuid/string_generator.hpp>
#include <helpers/container_plan.hpp>
#include <helpers/database.hpp>
#include <map>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <platform/http_client.hpp>
#include <views/api/v1/shipment_items/list/view.hpp>

namespace shipments::views::api::v1::shipment_items::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    try {
        platform::auth::CheckPermission(context, permissions::SHIPMENTS_READ);
        const auto& identity = platform::auth::GetIdentity(context);
        // Позиции и счётчики резервов читаются из одного снимка.
        auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, kReadSnapshot);
        const auto organization = platform::auth::GetOrganization(identity);
        if (transaction.Execute(sql::kGetShipment, organization, request.body.shipment_id)
                .IsEmpty())
            throw ApiError<404>{"not_found", "Поставка не найдена"};
        const auto rows =
            transaction.Execute(sql::kListItems, organization, request.body.shipment_id)
                .AsContainer<std::vector<pgtypes::ShipmentItemV1>>();
        const auto pending =
            transaction
                .Execute(sql::kPendingContainerCreations, organization, request.body.shipment_id)
                .AsContainer<std::vector<pgtypes::PendingContainerCreationV1>>();
        transaction.Commit();
        // Сетевой вызов не держит соединение и транзакцию Shipments.
        auto plan = ReadContainerPlan(deps, context, request.body.shipment_id, rows, pending);
        Response200 result;
        result.unplanned_items = std::move(plan.Unplanned);
        const auto after = request.body.after.has_value()
            ? std::optional{boost::uuids::string_generator{}(request.body.after.value())}
            : std::nullopt;
        for (const auto& row : rows) {
            if (after.has_value() && row.Id <= after.value())
                continue;
            if (result.items.size() >= static_cast<std::size_t>(request.body.limit))
                break;
            result.items.push_back(Convert(row, plan.Totals[{row.ContainerType, row.Ownership}]));
        }
        return result;
    } catch (const platform::http::CancelledError&) {
        throw;
    } catch (const platform::http::ApiError&) {
        return Response503{.error = "container_fact_unavailable",
            .message = "Не удалось получить фактические перевозки. Повторите загрузку"};
    } catch (const platform::http::TransportError&) {
        return Response503{.error = "container_fact_unavailable",
            .message = "Не удалось получить фактические перевозки. Повторите загрузку"};
    } catch (const platform::http::ContractError&) {
        return Response503{.error = "container_fact_unavailable",
            .message = "Не удалось получить фактические перевозки. Повторите загрузку"};
    }
}
}  // namespace shipments::views::api::v1::shipment_items::list
