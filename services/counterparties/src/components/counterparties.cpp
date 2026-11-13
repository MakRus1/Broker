#include <components/counterparties.hpp>
#include <converters/counterparties.hpp>
#include <converters/history.hpp>
#include <helpers/audit.hpp>
#include <helpers/counterparty_validation.hpp>
#include <models/pg/counterparty_event.hpp>
#include <models/pg/counterparty_raw.hpp>
#include <platform/auth/identity.hpp>
#include <platform/http_client.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace {
namespace models = counterparties::models;
namespace pg = userver::storages::postgres;
namespace sql = counterparties::sql;
using counterparties::ApiError;

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

}  // namespace

namespace counterparties {
using converters::Convert;
using helpers::WriteAudit;
namespace pg = userver::storages::postgres;

CounterpartiesComponent::CounterpartiesComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      organizations_(context.FindComponent<clients::organizations::Component>().GetClient()),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-counterparties")
                   .GetCluster()) {}

userver::yaml_config::Schema CounterpartiesComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

void CounterpartiesComponent::ValidateLogist(const std::string& organization,
    std::string_view token, const std::optional<std::string>& subject) const {
    if (!subject.has_value())
        return;

    bool exists;
    try {
        const auto response = organizations_.ApiOrganizationsEmployeesList(
            {.body = {.limit = 1, .organization_id = organization, .subject = subject}}, token);
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

models::Counterparty CounterpartiesComponent::CreateCounterparty(const std::string& organization,
    const platform::auth::Identity& identity, const models::CreateCounterpartyRequest& request,
    const models::Counterparty& value) const {
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

models::Counterparty CounterpartiesComponent::UpdateCounterparty(const std::string& organization,
    const platform::auth::Identity& identity,
    const models::UpdateCounterpartyRequest& request) const {
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

    const auto value = helpers::NormalizeAndValidateCounterparty(Convert(request, previous));
    CheckDuplicate(transaction, organization, value);

    const auto updated =
        transaction.Execute(sql::kUpdateCounterparty, organization, Convert(value));
    const auto result = Convert(updated.AsSingleRow<models::pg::CounterpartyRaw>());
    WriteAudit(transaction, organization, identity, "update_counterparty", result.id, request,
        result, std::optional{previous});
    transaction.Commit();

    return result;
}

models::Counterparty CounterpartiesComponent::FindCounterparty(
    const std::string& organization, const models::GetCounterpartyRequest& request) const {
    const auto rows = cluster_->Execute(
        pg::ClusterHostType::kMaster, sql::kGetCounterparty, organization, request.id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Контрагент не найден"};
    return Convert(rows.AsSingleRow<models::pg::CounterpartyRaw>());
}

models::Counterparty CounterpartiesComponent::SetCounterpartyArchived(
    const std::string& organization, const platform::auth::Identity& identity,
    const models::SetArchivedCounterpartyRequest& request) const {
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

std::vector<models::CounterpartyEvent> CounterpartiesComponent::GetCounterpartyHistory(
    const std::string& organization, const models::ListCounterpartyHistoryRequest& request) const {
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
