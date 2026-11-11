#include <components/financial_labels.hpp>
#include <helpers/financial_labels.hpp>
#include <helpers/registry_query.hpp>
#include <models/pg/financial_label_raw.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace finance {
using namespace helpers;

namespace {
models::pg::FinancialLabelRaw Prepare(const models::SaveFinancialLabelRequest& value) {
    if (value.expected_revision == std::numeric_limits<std::int64_t>::max())
        throw ApiError<400>{"invalid_revision", "Некорректная редакция финансовой метки"};
    const auto name = userver::utils::text::Trim(value.name);
    if (name.empty())
        throw ApiError<400>{"empty_name", "Укажите название финансовой метки"};
    if (value.vat_mode == models::VatMode::Exempt && value.vat_rate_basis_points.has_value())
        throw ApiError<400>{"unexpected_vat_rate", "Для «Без НДС» ставка не указывается"};
    if (value.vat_mode == models::VatMode::Rate && !value.vat_rate_basis_points.has_value())
        throw ApiError<400>{"vat_rate_required", "Укажите ставку НДС, включая нулевую"};
    auto note = value.note;
    if (note.has_value()) {
        note = userver::utils::text::Trim(note.value());
        if (note.value().empty())
            note.reset();
    }
    std::optional<std::int32_t> rate;
    if (value.vat_rate_basis_points.has_value())
        rate = static_cast<std::int32_t>(value.vat_rate_basis_points.value());
    return {.Id = boost::uuids::string_generator{}(value.id),
        .Name = name,
        .Direction = models::EnumValue(value.direction),
        .Unit = models::EnumValue(value.unit),
        .Currency = value.currency,
        .VatMode = models::EnumValue(value.vat_mode),
        .VatRateBasisPoints = rate,
        .Note = note,
        .Archived = value.archived,
        .Revision = value.expected_revision + 1};
}
}  // namespace

FinancialLabelsComponent::FinancialLabelsComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      deps_{.pg_cluster = context.FindComponent<userver::components::Postgres>("postgres-finance")
                              .GetCluster()} {}

userver::yaml_config::Schema FinancialLabelsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

FinancialLabelsComponent::FilterValuesResult FinancialLabelsComponent::FilterValues(
    const models::FinancialLabelFilterValuesRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    try {
        const auto organization =
            platform::auth::GetOrganization(platform::auth::GetIdentity(context));
        FilterValuesResult result;
        result.fields = registry::DescribeFields<models::RegistryField>(registry_fields::kFields);
        if (!request.field.has_value())
            return result;
        const auto query = platform::registry::BuildFilterValues(kRegistry, request, organization);
        const auto rows =
            deps.pg_cluster->Execute(
                               pg::ClusterHostType::kMaster, query.Statement, query.Parameters)
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

models::FinancialLabel FinancialLabelsComponent::Save(
    const models::SaveFinancialLabelRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_WRITE_FINANCIAL_LABELS);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto value = Prepare(request);
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto rows = request.expected_revision == 0
        ? transaction.Execute(sql::kCreateFinancialLabel, organization, value)
        : transaction.Execute(
              sql::kUpdateFinancialLabel, organization, value, request.expected_revision);
    if (rows.IsEmpty())
        throw ApiError<409>{"revision_conflict", "Метка изменена. Обновите карточку"};
    const auto result = Convert(rows.AsSingleRow<models::pg::FinancialLabelRaw>());
    transaction.Execute(sql::kWriteHistory, organization, result.id, result.revision,
        identity.ActorId(), userver::formats::json::ValueBuilder(result).ExtractValue());
    transaction.Commit();
    return result;
}

models::FinancialLabel FinancialLabelsComponent::Get(
    const models::GetFinancialLabelRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_FINANCIAL_LABELS);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto rows = deps.pg_cluster->Execute(
        pg::ClusterHostType::kMaster, sql::kGetFinancialLabel, organization, request.id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Финансовая метка не найдена"};
    return Convert(rows.AsSingleRow<models::pg::FinancialLabelRaw>());
}

FinancialLabelsComponent::ListResult FinancialLabelsComponent::List(
    const models::ListFinancialLabelsRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::FINANCE_READ_FINANCIAL_LABELS);
        const auto organization = platform::auth::GetOrganization(identity);
        const auto& body = request;
        const auto scope = platform::registry::CursorScope(body, organization);
        if (body.cursor.has_value() && body.cursor.value().scope != scope)
            throw ApiError<400>{
                "invalid_cursor", "Курсор не соответствует фильтрам или сортировке"};
        const auto query = platform::registry::BuildPage(kRegistry, body, organization);
        // Транзакция ограничивает SET LOCAL; HTTP-вызовов и записи внутри нет.
        auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster,
            pg::TransactionOptions{
                pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
        transaction.Execute(sql::kRegistryPlan);
        auto rows = transaction.Execute(query.Statement, query.Parameters)
                        .AsContainer<std::vector<RegistryRow>>(pg::kRowTag);
        const bool more = rows.size() > static_cast<std::size_t>(body.limit);
        if (more)
            rows.pop_back();
        std::optional<models::FinancialLabelRegistryCursor> cursor;
        if (more)
            cursor = models::FinancialLabelRegistryCursor{
                .id = boost::uuids::to_string(rows.back().Label.Id),
                .scope = scope,
                .value = rows.back().SortKey};
        transaction.Commit();
        return ListResult{.items = Convert(rows), .next_cursor = cursor};
    } catch (const platform::registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}
}  // namespace finance
