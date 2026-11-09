#include <components/cards.hpp>
#include <components/internal/settings.hpp>
#include <registry/fields.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace sales {
using namespace detail;

CardsComponent::CardsComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      deps_{
          .counterparties = context.FindComponent<clients::counterparties::Component>().GetClient(),
          .organizations = context.FindComponent<clients::organizations::Component>().GetClient(),
          .pg_cluster =
              context.FindComponent<userver::components::Postgres>("postgres-sales").GetCluster()} {
}

userver::yaml_config::Schema CardsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::SalesCard CardsComponent::Save(const models::SaveSalesCardRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    const auto& value = request;
    ValidateReferences(deps, context, value.customer_id, value.manager_subject);
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    transaction.Execute(sql::kLockSettings, organization);
    const auto previous = transaction.Execute(sql::kLockCards, organization, value.id);
    const auto previous_stage =
        previous.IsEmpty() ? std::nullopt : std::optional{previous.AsSingleRow<std::string>()};
    ValidateStage(Settings(transaction.Execute(sql::kGetSettings, organization)), value.stage_id,
        previous_stage);
    const auto document = json::ValueBuilder(value).ExtractValue();
    transaction.Execute(sql::kClearChangedActionSource, organization, document);
    const auto changed = value.expected_revision == 0
        ? transaction.Execute(sql::kCreateCards, organization, document)
        : transaction.Execute(sql::kUpdateCards, organization, document, value.expected_revision);
    if (changed.IsEmpty())
        throw ApiError<409>{
            "revision_conflict", "Запись уже существует, изменена или передана в поставку"};
    const auto rows = transaction.Execute(sql::kGetCards, organization, value.id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Запись не найдена в организации"};
    const auto result = models::ParseModelJson(
        rows.AsSingleRow<json::Value>(), std::type_identity<models::SalesCard>{});
    transaction.Execute(sql::kWriteHistory, organization, std::string{"cards"}, result.id,
        result.revision, platform::auth::GetIdentity(context).ActorId(),
        json::ValueBuilder(result).ExtractValue());
    transaction.Commit();
    return result;
}

models::SalesCard CardsComponent::Get(
    const models::IdRequest& request, userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kGetCards,
        platform::auth::GetOrganization(platform::auth::GetIdentity(context)), request.id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Запись не найдена в организации"};
    return models::ParseModelJson(
        rows.AsSingleRow<json::Value>(), std::type_identity<models::SalesCard>{});
}

CardsComponent::ListResult CardsComponent::List(const models::ListSalesRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    try {
        const auto& body = request;
        const auto organization =
            platform::auth::GetOrganization(platform::auth::GetIdentity(context));
        const auto scope = registry::CursorScope(body, organization);
        const bool date_sort =
            registry::RegistryOrderBy(kCardsRegistry.Fields, body.sort).CursorType ==
            registry::CursorKeyType::Date;
        if (body.cursor.has_value()) {
            const auto& cursor = body.cursor.value();
            if (cursor.scope != scope || date_sort != cursor.date_value.has_value() ||
                date_sort == cursor.text_value.has_value())
                throw ApiError<400>{
                    "invalid_cursor", "Курсор не соответствует фильтрам или сортировке"};
        }
        const auto query = registry::BuildPage(kCardsRegistry, body, organization);
        auto rows = deps.pg_cluster
                        ->Execute(pg::ClusterHostType::kMaster, query.Statement, query.Parameters)
                        .AsContainer<std::vector<RegistryRow>>(pg::kRowTag);
        const bool more = rows.size() > static_cast<std::size_t>(body.limit);
        if (more)
            rows.pop_back();
        ListResult response;
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

std::vector<models::SalesHistoryEntry> CardsComponent::History(
    const models::HistoryRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kHistoryCards,
        platform::auth::GetOrganization(platform::auth::GetIdentity(context)), request.id,
        request.after_revision.value_or(0));
    std::vector<models::SalesHistoryEntry> response;
    response.reserve(rows.Size());
    for (const auto& row : rows.AsSetOf<json::Value>())
        response.push_back(
            models::ParseModelJson(row, std::type_identity<models::SalesHistoryEntry>{}));
    return response;
}

CardsComponent::FilterValuesResult CardsComponent::FilterValues(
    const models::SalesFilterValuesRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    try {
        FilterValuesResult result;
        result.fields = registry::DescribeFields<models::RegistryField>(kCardsRegistry.Fields);
        if (!request.field.has_value())
            return result;
        const auto query = registry::BuildFilterValues(kCardsRegistry, request,
            platform::auth::GetOrganization(platform::auth::GetIdentity(context)));
        const auto rows =
            deps.pg_cluster->Execute(
                               pg::ClusterHostType::kMaster, query.Statement, query.Parameters)
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
