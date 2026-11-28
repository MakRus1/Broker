#include <components/counterparty_registry.hpp>
#include <converters/counterparties.hpp>
#include <models/counterparty_registry_row.hpp>
#include <models/errors.hpp>
#include <platform/db.hpp>
#include <platform/registry_read.hpp>
#include <registry/query.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>

namespace counterparties {
namespace pg = userver::storages::postgres;
using converters::Convert;
using registry_query::kRegistry;

CounterpartyRegistry::CounterpartyRegistry(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-counterparties")
                   .GetCluster()) {}

models::CounterpartyListResult CounterpartyRegistry::ListCounterparties(
    const boost::uuids::uuid& organization,
    const models::ListCounterpartiesRequest& request) const {
    try {
        const auto& body = request;
        const auto scope = platform::registry::CursorScope(body, organization);
        const auto order = registry::RegistryOrderBy(registry_fields::kFields, body.sort);
        if (body.after.has_value() && (body.cursor.has_value() || !order.ById || body.descending))
            throw ApiError<400>{"invalid_cursor", "Для выбранной сортировки используйте cursor"};
        platform::registry::ValidateListCursor(body.cursor, scope);

        const auto query = platform::registry::BuildPage(kRegistry, body, organization);
        // Транзакция ограничивает SET LOCAL; HTTP-вызовов и записи внутри нет.
        auto transaction =
            cluster_->Begin(pg::ClusterHostType::kMaster, platform::db::kReadOnlySnapshot);
        auto rows = platform::registry::ExecuteWithCustomPlan(transaction, query)
                        .AsContainer<std::vector<models::CounterpartyRegistryRow>>(pg::kRowTag);
        const auto cursor = platform::registry::FinishListPage<models::CounterpartyRegistryCursor>(
            rows, body.limit, scope, [](const auto& row) { return row.Counterparty.Id; });
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
    const boost::uuids::uuid& organization,
    const models::CounterpartyFilterValuesRequest& request) const {
    try {
        models::CounterpartyFilterValuesResult result;
        result.fields = registry::DescribeFields<models::RegistryField>(registry_fields::kFields);
        if (!request.field.has_value())
            return result;
        const auto query = platform::registry::BuildFilterValues(kRegistry, request, organization);
        const auto rows =
            cluster_->Execute(pg::ClusterHostType::kMaster, query.Statement, query.Parameters)
                .AsContainer<std::vector<registry::ValueRow>>(pg::kRowTag);
        platform::registry::FillFilterValues(result, rows, request.limit);
        return result;
    } catch (const registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

}  // namespace counterparties
