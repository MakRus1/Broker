#include <components/sales_registry.hpp>
#include <models/registry_row.hpp>
#include <registry/fields.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace sales {
namespace pg = userver::storages::postgres;
namespace json = userver::formats::json;
namespace registry = platform::registry;

SalesRegistry::SalesRegistry(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-sales").GetCluster()) {}

userver::yaml_config::Schema SalesRegistry::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::RegistryPage<models::SalesCard> SalesRegistry::ListCards(
    const std::string& organization, const models::ListSalesRequest& request) const {
    try {
        const auto& body = request;
        const auto scope = registry::CursorScope(body, organization);
        const bool date_sort =
            registry::RegistryOrderBy(registry_query::kCardsRegistry.Fields, body.sort)
                .CursorType == registry::CursorKeyType::Date;
        if (body.cursor.has_value()) {
            const auto& cursor = body.cursor.value();
            if (cursor.scope != scope || date_sort != cursor.date_value.has_value() ||
                date_sort == cursor.text_value.has_value())
                throw ApiError<400>{
                    "invalid_cursor", "Курсор не соответствует фильтрам или сортировке"};
        }
        const auto query = registry::BuildPage(registry_query::kCardsRegistry, body, organization);
        auto rows =
            cluster_->Execute(pg::ClusterHostType::kMaster, query.Statement, query.Parameters)
                .AsContainer<std::vector<models::RegistryRow>>(pg::kRowTag);
        const bool more = rows.size() > static_cast<std::size_t>(body.limit);
        if (more)
            rows.pop_back();
        models::RegistryPage<models::SalesCard> response;
        response.items.reserve(rows.size());
        for (const auto& row : rows)
            response.items.push_back(
                models::ParseModelJson(row.Document, std::type_identity<models::SalesCard>{}));
        if (more) {
            const auto& last = rows.back();
            response.next_cursor =
                models::SalesRegistryCursor{.date_value = date_sort ? last.DateKey : std::nullopt,
                    .id = response.items.back().id,
                    .scope = scope,
                    .text_value = date_sort ? std::nullopt : std::optional{last.TextKey}};
        }
        return response;
    } catch (const registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

models::RegistryFilterValues SalesRegistry::GetCardsFilterValues(
    const std::string& organization, const models::SalesFilterValuesRequest& request) const {
    try {
        models::RegistryFilterValues result;
        result.fields =
            registry::DescribeFields<models::RegistryField>(registry_query::kCardsRegistry.Fields);
        if (!request.field.has_value())
            return result;
        const auto query =
            registry::BuildFilterValues(registry_query::kCardsRegistry, request, organization);
        const auto rows =
            cluster_->Execute(pg::ClusterHostType::kMaster, query.Statement, query.Parameters)
                .AsContainer<std::vector<registry::ValueRow>>(pg::kRowTag);
        for (std::size_t i = 0; i < rows.size() && i < static_cast<std::size_t>(request.limit); ++i)
            result.values.push_back({.label = rows[i].Label, .value = rows[i].Value});
        if (rows.size() > static_cast<std::size_t>(request.limit))
            result.next_after = result.values.back().value;
        return result;
    } catch (const registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

models::RegistryPage<models::Inquiry> SalesRegistry::ListInquiries(
    const std::string& organization, const models::ListSalesRequest& request) const {
    try {
        const auto& body = request;
        const auto scope = registry::CursorScope(body, organization);
        const bool date_sort =
            registry::RegistryOrderBy(registry_query::kInquiriesRegistry.Fields, body.sort)
                .CursorType == registry::CursorKeyType::Date;
        if (body.cursor.has_value()) {
            const auto& cursor = body.cursor.value();
            if (cursor.scope != scope || date_sort != cursor.date_value.has_value() ||
                date_sort == cursor.text_value.has_value())
                throw ApiError<400>{
                    "invalid_cursor", "Курсор не соответствует фильтрам или сортировке"};
        }
        const auto query =
            registry::BuildPage(registry_query::kInquiriesRegistry, body, organization);
        auto rows =
            cluster_->Execute(pg::ClusterHostType::kMaster, query.Statement, query.Parameters)
                .AsContainer<std::vector<models::RegistryRow>>(pg::kRowTag);
        const bool more = rows.size() > static_cast<std::size_t>(body.limit);
        if (more)
            rows.pop_back();
        models::RegistryPage<models::Inquiry> response;
        response.items.reserve(rows.size());
        for (const auto& row : rows)
            response.items.push_back(
                models::ParseModelJson(row.Document, std::type_identity<models::Inquiry>{}));
        if (more) {
            const auto& last = rows.back();
            response.next_cursor =
                models::SalesRegistryCursor{.date_value = date_sort ? last.DateKey : std::nullopt,
                    .id = response.items.back().id,
                    .scope = scope,
                    .text_value = date_sort ? std::nullopt : std::optional{last.TextKey}};
        }
        return response;
    } catch (const registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

models::RegistryFilterValues SalesRegistry::GetInquiriesFilterValues(
    const std::string& organization, const models::SalesFilterValuesRequest& request) const {
    try {
        models::RegistryFilterValues result;
        result.fields = registry::DescribeFields<models::RegistryField>(
            registry_query::kInquiriesRegistry.Fields);
        if (!request.field.has_value())
            return result;
        const auto query =
            registry::BuildFilterValues(registry_query::kInquiriesRegistry, request, organization);
        const auto rows =
            cluster_->Execute(pg::ClusterHostType::kMaster, query.Statement, query.Parameters)
                .AsContainer<std::vector<registry::ValueRow>>(pg::kRowTag);
        for (std::size_t i = 0; i < rows.size() && i < static_cast<std::size_t>(request.limit); ++i)
            result.values.push_back({.label = rows[i].Label, .value = rows[i].Value});
        if (rows.size() > static_cast<std::size_t>(request.limit))
            result.next_after = result.values.back().value;
        return result;
    } catch (const registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

}  // namespace sales
