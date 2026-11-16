#include <components/sales_registry.hpp>
#include <models/registry_row.hpp>
#include <registry/fields.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace {
namespace models = sales::models;
using sales::ApiError;
namespace pg = userver::storages::postgres;
namespace registry = platform::registry;

template <typename Item>
models::RegistryPage<Item> ReadPage(const pg::ClusterPtr& cluster,
    const registry::Definition<models::ListSalesRequest, boost::uuids::uuid>& definition,
    const boost::uuids::uuid& organization, const models::ListSalesRequest& request) {
    try {
        const auto scope = registry::CursorScope(request, organization);
        const bool date_sort =
            registry::RegistryOrderBy(definition.Fields, request.sort).CursorType ==
            registry::CursorKeyType::Date;
        registry::ValidateListCursor(request.cursor, scope, date_sort);

        const auto query = registry::BuildPage(definition, request, organization);
        auto rows =
            cluster->Execute(pg::ClusterHostType::kMaster, query.Statement, query.Parameters)
                .AsContainer<std::vector<models::RegistryRow>>(pg::kRowTag);
        models::RegistryPage<Item> response;
        response.next_cursor = registry::FinishListPage<models::SalesRegistryCursor>(
            rows, request.limit, scope, [](const auto& row) { return row.Id; }, date_sort);
        response.items.reserve(rows.size());
        for (const auto& row : rows)
            response.items.push_back(
                models::ParseModelJson(row.Document, std::type_identity<Item>{}));

        return response;
    } catch (const registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

models::RegistryFilterValues ReadFilterValues(const pg::ClusterPtr& cluster,
    const registry::Definition<models::ListSalesRequest, boost::uuids::uuid>& definition,
    const boost::uuids::uuid& organization, const models::SalesFilterValuesRequest& request) {
    try {
        models::RegistryFilterValues result;
        result.fields = registry::DescribeFields<models::RegistryField>(definition.Fields);
        if (!request.field.has_value())
            return result;

        const auto query = registry::BuildFilterValues(definition, request, organization);
        const auto rows =
            cluster->Execute(pg::ClusterHostType::kMaster, query.Statement, query.Parameters)
                .AsContainer<std::vector<registry::ValueRow>>(pg::kRowTag);
        platform::registry::FillFilterValues(result, rows, request.limit);
        return result;
    } catch (const registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

}  // namespace

namespace sales {
namespace pg = userver::storages::postgres;

SalesRegistry::SalesRegistry(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-sales").GetCluster()) {}

userver::yaml_config::Schema SalesRegistry::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::RegistryPage<models::SalesCard> SalesRegistry::ListCards(
    const boost::uuids::uuid& organization, const models::ListSalesRequest& request) const {
    return ReadPage<models::SalesCard>(
        cluster_, registry_query::kCardsRegistry, organization, request);
}

models::RegistryFilterValues SalesRegistry::GetCardsFilterValues(
    const boost::uuids::uuid& organization, const models::SalesFilterValuesRequest& request) const {
    return ReadFilterValues(cluster_, registry_query::kCardsRegistry, organization, request);
}

models::RegistryPage<models::Inquiry> SalesRegistry::ListInquiries(
    const boost::uuids::uuid& organization, const models::ListSalesRequest& request) const {
    return ReadPage<models::Inquiry>(
        cluster_, registry_query::kInquiriesRegistry, organization, request);
}

models::RegistryFilterValues SalesRegistry::GetInquiriesFilterValues(
    const boost::uuids::uuid& organization, const models::SalesFilterValuesRequest& request) const {
    return ReadFilterValues(cluster_, registry_query::kInquiriesRegistry, organization, request);
}

}  // namespace sales
