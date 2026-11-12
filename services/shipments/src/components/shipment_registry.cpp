#include <components/shipment_registry.hpp>
#include <converters/registry.hpp>
#include <helpers/database.hpp>
#include <models/errors.hpp>
#include <models/pg/shipment_registry_row.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <registry/query.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

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

userver::yaml_config::Schema ShipmentRegistryComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::ShipmentsFilterValuesResult ShipmentRegistryComponent::GetShipmentFilterValues(
    const models::ShipmentsFilterValuesRequest& request,
    userver::server::request::RequestContext& context) const {
    try {
        const auto organization =
            platform::auth::GetOrganization(platform::auth::GetIdentity(context));
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
        const bool more = rows.size() > static_cast<std::size_t>(request.limit);
        for (std::size_t index = 0;
             index < rows.size() && index < static_cast<std::size_t>(request.limit); ++index)
            result.values.push_back({.label = rows[index].Label, .value = rows[index].Value});
        if (more)
            result.next_after = result.values.back().value;
        return result;
    } catch (const registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

models::ShipmentsListResult ShipmentRegistryComponent::FindShipments(
    const models::ListShipmentsRequest& request,
    userver::server::request::RequestContext& context) const {
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::SHIPMENTS_READ);
        const auto organization = platform::auth::GetOrganization(identity);
        const auto scope = platform::registry::CursorScope(request, organization);
        const auto order =
            registry::RegistryOrderBy(registry_query::registry_fields::kFields, request.sort);
        const bool date_sort = order.CursorType == registry::CursorKeyType::Date;
        if (request.after.has_value() &&
            (request.cursor.has_value() || !order.ById || request.descending))
            throw ApiError<400>{"invalid_cursor", "Для выбранной сортировки используйте cursor"};
        if (request.cursor.has_value()) {
            const auto& cursor = request.cursor.value();
            if (cursor.scope != scope || date_sort != cursor.date_value.has_value() ||
                date_sort == cursor.text_value.has_value())
                throw ApiError<400>{
                    "invalid_cursor", "Курсор не соответствует фильтрам или сортировке"};
        }
        const auto query =
            platform::registry::BuildPage(registry_query::kRegistry, request, organization);
        // Транзакция ограничивает SET LOCAL; HTTP-вызовов и записи внутри нет.
        auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, helpers::kReadSnapshot);
        transaction.Execute(sql::kRegistryPlan);
        auto rows = transaction.Execute(query.Statement, query.Parameters)
                        .AsContainer<std::vector<models::pg::ShipmentRegistryRow>>(pg::kRowTag);
        const bool more = rows.size() > static_cast<std::size_t>(request.limit);
        if (more)
            rows.pop_back();
        std::optional<models::ShipmentRegistryCursor> cursor;
        if (more) {
            const auto& last = rows.back();
            cursor = models::ShipmentRegistryCursor{
                .date_value = date_sort ? last.DateKey : std::nullopt,
                .id = boost::uuids::to_string(last.Shipment.Id),
                .scope = scope,
                .text_value = date_sort ? std::nullopt : std::optional{last.TextKey}};
        }
        transaction.Commit();
        return models::ShipmentsListResult{
            .items = converters::Convert(rows), .next_cursor = cursor};
    } catch (const platform::registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

}  // namespace shipments
