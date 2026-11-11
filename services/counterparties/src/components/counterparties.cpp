#include <algorithm>
#include <cctype>
#include <components/counterparties.hpp>
#include <converters/counterparties.hpp>
#include <converters/history.hpp>
#include <helpers/audit.hpp>
#include <helpers/normalization.hpp>
#include <limits>
#include <models/pg/counterparty_event.hpp>
#include <models/pg/counterparty_raw.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <platform/contact_validation.hpp>
#include <platform/http_client.hpp>
#include <platform/registry_page.hpp>
#include <registry/query.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace counterparties {
using namespace helpers;
using converters::Convert;
using registry_query::kRegistry;

namespace {
models::Counterparty NormalizeAndValidate(models::Counterparty value) {
    value.name = userver::utils::text::Trim(value.name);
    value.legal_name = Normalize(value.legal_name);
    value.country_code = Normalize(value.country_code);
    if (value.country_code.has_value()) {
        auto& country = value.country_code.value();
        std::transform(country.begin(), country.end(), country.begin(),
            [](unsigned char c) { return std::toupper(c); });
    }
    value.tax_id = Normalize(value.tax_id);
    value.kpp.emplace(Normalize(value.kpp.value_or(std::nullopt)));
    value.logist.emplace(value.logist.value_or(std::nullopt));
    value.registration_id = Normalize(value.registration_id);
    value.legal_address = Normalize(value.legal_address);
    std::sort(value.roles.begin(), value.roles.end());
    if (value.name.empty())
        throw ApiError<400>{"empty_name", "Укажите название контрагента"};
    if (value.tax_id.has_value() && !value.country_code.has_value())
        throw ApiError<400>{"country_required", "Укажите страну для налогового номера"};
    for (auto& contact : value.contacts) {
        contact.name = userver::utils::text::Trim(contact.name);
        contact.position.emplace(Normalize(contact.position.value_or(std::nullopt)));
        const auto email = Normalize(contact.email.value_or(std::nullopt));
        contact.email.emplace(email);
        const auto phone = Normalize(contact.phone.value_or(std::nullopt));
        contact.phone.emplace(phone);
        if (phone.has_value() && !platform::IsPhone(phone.value()))
            throw ApiError<400>{"invalid_phone", "Телефон должен содержать от 7 до 15 цифр"};
        if (contact.name.empty())
            throw ApiError<400>{"empty_contact", "Укажите имя контактного лица"};
        if (email.has_value() && !platform::IsEmail(email.value()))
            throw ApiError<400>{"invalid_email", "Проверьте email контактного лица"};
    }
    return value;
}

void CheckDuplicate(pg::Transaction& transaction, const std::string& organization,
    const models::Counterparty& value) {
    if (!value.tax_id.has_value())
        return;
    transaction.Execute(sql::kLockRequisites,
        organization + ":" + value.country_code.value() + ":" + value.tax_id.value());
    const auto duplicate = transaction.Execute(
        sql::kFindDuplicate, organization, value.country_code, value.tax_id, value.id);
    if (!duplicate.IsEmpty())
        throw ApiError<409>{"duplicate_requisites",
            "Такие реквизиты уже есть у контрагента: " + duplicate.AsSingleRow<std::string>()};
}

struct RegistryRow {
    models::pg::CounterpartyRaw Counterparty;
    std::string SortKey;
};

}  // namespace

CounterpartiesComponent::CounterpartiesComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      organizations_(context.FindComponent<clients::organizations::Component>().GetClient()),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-counterparties")
                   .GetCluster()) {}

userver::yaml_config::Schema CounterpartiesComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

void CounterpartiesComponent::ValidateLogist(
    const userver::server::request::RequestContext& context,
    const std::optional<std::string>& subject) const {
    if (!subject.has_value())
        return;
    const auto& identity = platform::auth::GetIdentity(context);
    bool exists;
    try {
        const auto response = organizations_.ApiOrganizationsEmployeesList(
            {.body = {.limit = 1,
                 .organization_id = platform::auth::GetOrganization(identity),
                 .subject = subject}},
            platform::auth::GetAccessToken(context));
        exists = !response.items.empty();
    } catch (const platform::http::ApiError&) {
        throw ApiError<503>{"employee_directory_unavailable",
            "Не удалось проверить сотрудника. Повторите сохранение позже"};
    } catch (const platform::http::CancelledError&) {
        throw;
    } catch (const platform::http::TransportError&) {
        throw ApiError<503>{"employee_directory_unavailable",
            "Не удалось проверить сотрудника. Повторите сохранение позже"};
    } catch (const platform::http::ContractError&) {
        throw ApiError<503>{
            "employee_directory_unavailable", "Некорректный ответ справочника сотрудников"};
    }

    if (!exists)
        throw ApiError<400>{"invalid_logist", "Выберите действующего сотрудника организации"};
}

models::Counterparty CounterpartiesComponent::Create(
    const models::CreateCounterpartyRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::COUNTERPARTIES_WRITE);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto value = NormalizeAndValidate(Convert(request));
    if (request.logist.has_value())
        ValidateLogist(context, request.logist.value());
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    transaction.Execute(sql::kLockCounterpartyIdentity, organization, value.id);
    CheckDuplicate(transaction, organization, value);
    const auto inserted =
        transaction.Execute(sql::kCreateCounterparty, organization, Convert(value));
    // UUID делает безопасным повтор создания после потери ответа.
    const auto rows = inserted.IsEmpty()
        ? transaction.Execute(sql::kGetCounterparty, organization, value.id)
        : inserted;
    const auto result = Convert(rows.AsSingleRow<models::pg::CounterpartyRaw>());
    if (userver::formats::json::ValueBuilder(result).ExtractValue() !=
        userver::formats::json::ValueBuilder(value).ExtractValue())
        throw ApiError<409>{"id_conflict", "Идентификатор занят другим контрагентом"};
    if (!inserted.IsEmpty())
        WriteAudit(
            transaction, organization, identity, "create_counterparty", result.id, request, result);
    transaction.Commit();
    return result;
}

models::Counterparty CounterpartiesComponent::Update(
    const models::UpdateCounterpartyRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::COUNTERPARTIES_WRITE);
    const auto organization = platform::auth::GetOrganization(identity);
    if (request.logist.has_value())
        ValidateLogist(context, request.logist.value());
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto id = boost::uuids::to_string(boost::uuids::string_generator{}(request.id));
    transaction.Execute(sql::kLockCounterpartyIdentity, organization, id);
    const auto rows = transaction.Execute(sql::kLockCounterparty, organization, id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Контрагент не найден"};
    const auto previous = Convert(rows.AsSingleRow<models::pg::CounterpartyRaw>());
    if (previous.revision != request.expected_revision)
        throw ApiError<409>{
            "revision_conflict", "Контрагент изменился. Обновите карточку перед редактированием"};
    if (previous.archived)
        throw ApiError<409>{"archived", "Сначала восстановите контрагента из архива"};
    const auto value = NormalizeAndValidate(Convert(request, previous));
    CheckDuplicate(transaction, organization, value);
    const auto updated =
        transaction.Execute(sql::kUpdateCounterparty, organization, Convert(value));
    const auto result = Convert(updated.AsSingleRow<models::pg::CounterpartyRaw>());
    WriteAudit(transaction, organization, identity, "update_counterparty", result.id, request,
        result, std::optional{previous});
    transaction.Commit();
    return result;
}

models::Counterparty CounterpartiesComponent::Get(const models::GetCounterpartyRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::COUNTERPARTIES_READ);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto rows = cluster_->Execute(
        pg::ClusterHostType::kMaster, sql::kGetCounterparty, organization, request.id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Контрагент не найден"};
    return Convert(rows.AsSingleRow<models::pg::CounterpartyRaw>());
}

models::Counterparty CounterpartiesComponent::SetArchived(
    const models::SetArchivedCounterpartyRequest& request,
    userver::server::request::RequestContext& context) const {
    platform::auth::CheckPermission(context, permissions::COUNTERPARTIES_WRITE);
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto rows = transaction.Execute(sql::kLockCounterparty, organization, request.id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Контрагент не найден"};
    const auto previous = Convert(rows.AsSingleRow<models::pg::CounterpartyRaw>());
    if (previous.revision != request.expected_revision)
        throw ApiError<409>{"revision_conflict", "Контрагент изменился. Обновите карточку"};
    if (previous.archived == request.archived) {
        transaction.Commit();
        return previous;
    }
    const auto result =
        Convert(transaction.Execute(sql::kSetArchived, organization, request.id, request.archived)
                    .AsSingleRow<models::pg::CounterpartyRaw>());
    WriteAudit(transaction, organization, identity,
        request.archived ? "archive_counterparty" : "restore_counterparty", result.id, request,
        result, std::optional{previous});
    transaction.Commit();
    return result;
}

CounterpartiesComponent::ListResult CounterpartiesComponent::List(
    const models::ListCounterpartiesRequest& request,
    userver::server::request::RequestContext& context) const {
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::COUNTERPARTIES_READ);
        const auto organization = platform::auth::GetOrganization(identity);
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
                        .AsContainer<std::vector<RegistryRow>>(pg::kRowTag);
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
        return ListResult{.items = std::move(items), .next_cursor = cursor};
    } catch (const platform::registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

CounterpartiesComponent::FilterValuesResult CounterpartiesComponent::FilterValues(
    const models::CounterpartyFilterValuesRequest& request,
    userver::server::request::RequestContext& context) const {
    try {
        const auto organization =
            platform::auth::GetOrganization(platform::auth::GetIdentity(context));
        FilterValuesResult result;
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

std::vector<models::CounterpartyEvent> CounterpartiesComponent::History(
    const models::ListCounterpartyHistoryRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::COUNTERPARTIES_READ);
    const auto organization = platform::auth::GetOrganization(identity);
    // Физического удаления карточек нет; история читается отдельным атомарным запросом.
    if (cluster_
            ->Execute(pg::ClusterHostType::kMaster, sql::kGetCounterparty, organization, request.id)
            .IsEmpty())
        throw ApiError<404>{"not_found", "Контрагент не найден"};
    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kListHistory,
        organization, request.id, request.before, request.limit);
    return Convert(rows.AsContainer<std::vector<models::pg::CounterpartyEvent>>());
}
}  // namespace counterparties
