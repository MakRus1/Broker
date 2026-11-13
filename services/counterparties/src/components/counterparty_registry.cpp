#include <components/counterparty_registry.hpp>
#include <converters/counterparties.hpp>
#include <models/counterparty_registry_row.hpp>
#include <models/errors.hpp>
#include <registry/query.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace counterparties {
namespace pg = userver::storages::postgres;
using converters::Convert;
using registry_query::kRegistry;

CounterpartyRegistry::CounterpartyRegistry(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-counterparties")
                   .GetCluster()) {}

userver::yaml_config::Schema CounterpartyRegistry::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::CounterpartyListResult CounterpartyRegistry::ListCounterparties(
    const std::string& organization, const models::ListCounterpartiesRequest& request) const {
    try {
        const auto& body = request;
        const auto scope = platform::registry::CursorScope(body, organization);
        const auto order = registry::RegistryOrderBy(registry_fields::kFields, body.sort);
        if (body.after.has_value() && (body.cursor.has_value() || !order.ById || body.descending))
            throw ApiError<400>{"invalid_cursor", "Для выбранной сортировки используйте cursor"};
        if (body.cursor.has_value() && body.cursor.value().scope != scope)
            throw ApiError<400>{
                "invalid_cursor", "Курсор не соответствует фильтрам или сортировке"};

        const auto query = platform::registry::BuildPage(kRegistry, body, organization);
        // Транзакция ограничивает SET LOCAL; HTTP-вызовов и записи внутри нет.
        auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster,
            pg::TransactionOptions{
                pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
        transaction.Execute(sql::kRegistryPlan);
        auto rows = transaction.Execute(query.Statement, query.Parameters)
                        .AsContainer<std::vector<models::CounterpartyRegistryRow>>(pg::kRowTag);
        const bool more = rows.size() > static_cast<std::size_t>(body.limit);
        if (more)
            rows.pop_back();
        std::optional<models::CounterpartyRegistryCursor> cursor;
        if (more)
            cursor = models::CounterpartyRegistryCursor{
                .id = boost::uuids::to_string(rows.back().Counterparty.Id),
                .scope = scope,
                .value = rows.back().SortKey};
        transaction.Commit();

        std::vector<models::Counterparty> items;
        items.reserve(rows.size());
        for (const auto& row : rows)
            items.push_back(Convert(row.Counterparty));
        return models::CounterpartyListResult{.items = std::move(items), .next_cursor = cursor};
    } catch (const platform::registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

models::CounterpartyFilterValuesResult CounterpartyRegistry::GetCounterpartyFilterValues(
    const std::string& organization, const models::CounterpartyFilterValuesRequest& request) const {
    try {
        models::CounterpartyFilterValuesResult result;
        result.fields = registry::DescribeFields<models::RegistryField>(registry_fields::kFields);
        if (!request.field.has_value())
            return result;
        const auto query = platform::registry::BuildFilterValues(kRegistry, request, organization);
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

}  // namespace counterparties
