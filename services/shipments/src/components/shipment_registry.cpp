#include <components/shipment_registry.hpp>
#include <converters/registry.hpp>
#include <helpers/database.hpp>
#include <models/errors.hpp>
#include <models/shipment_registry_row.hpp>
#include <platform/registry_read.hpp>
#include <registry/query.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>

namespace shipments {
namespace pg = userver::storages::postgres;
namespace registry = platform::registry;

ShipmentRegistryComponent::ShipmentRegistryComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-shipments").GetCluster()) {
}

models::ShipmentsFilterValuesResult ShipmentRegistryComponent::GetShipmentFilterValues(
    const models::ShipmentsFilterValuesRequest& request,
    const boost::uuids::uuid& organization) const {
    try {
        models::ShipmentsFilterValuesResult result;
        result.fields = registry::DescribeFields<models::RegistryField>(
            registry_query::registry_fields::kFields);
        if (!request.field.has_value())
            return result;
        const auto query =
            platform::registry::BuildFilterValues(registry_query::kRegistry, request, organization);
        const auto rows =
            cluster_->Execute(pg::ClusterHostType::kMaster, query.Statement, query.Parameters)
                .AsContainer<std::vector<registry::ValueRow>>(pg::kRowTag);
        platform::registry::FillFilterValues(result, rows, request.limit);
        return result;
    } catch (const registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

models::ShipmentsListResult ShipmentRegistryComponent::FindShipments(
    const models::ListShipmentsRequest& request, const boost::uuids::uuid& organization) const {
    try {
        const auto scope = platform::registry::CursorScope(request, organization);
        const auto order =
            registry::RegistryOrderBy(registry_query::registry_fields::kFields, request.sort);
        const bool date_sort = order.CursorType == registry::CursorKeyType::Date;
        if (request.after.has_value() &&
            (request.cursor.has_value() || !order.ById || request.descending))
            throw ApiError<400>{"invalid_cursor", "Для выбранной сортировки используйте cursor"};
        platform::registry::ValidateListCursor(request.cursor, scope, date_sort);
        const auto query =
            platform::registry::BuildPage(registry_query::kRegistry, request, organization);
        // Транзакция ограничивает SET LOCAL; HTTP-вызовов и записи внутри нет.
        auto transaction =
            cluster_->Begin(pg::ClusterHostType::kMaster, helpers::kReadOnlySnapshotTransaction);
        auto rows = platform::registry::ExecuteWithCustomPlan(transaction, query)
                        .AsContainer<std::vector<models::ShipmentRegistryRow>>(pg::kRowTag);
        const auto cursor = platform::registry::FinishListPage<models::ShipmentRegistryCursor>(
            rows, request.limit, scope, [](const auto& row) { return row.Shipment.Id; }, date_sort);
        transaction.Commit();
        return models::ShipmentsListResult{
            .items = converters::Convert(rows), .next_cursor = cursor};
    } catch (const platform::registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

}  // namespace shipments
