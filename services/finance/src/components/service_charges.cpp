#include <components/invoices.hpp>
#include <components/service_charges.hpp>
#include <converters/invoice.hpp>
#include <converters/service_charge_input.hpp>
#include <finance/sql_queries.hpp>
#include <helpers/document_parties.hpp>
#include <helpers/exchange_amounts.hpp>
#include <helpers/finance_settings.hpp>
#include <helpers/history/snapshot.hpp>
#include <helpers/service_charges.hpp>
#include <map>
#include <models/errors.hpp>
#include <models/pg/financial_label_raw.hpp>
#include <models/pg/service_charge_raw.hpp>
#include <models/reporting_charge_row.hpp>
#include <models/service_charge_total_row.hpp>
#include <platform/auth/identity.hpp>
#include <platform/charge_amounts.hpp>
#include <platform/date_validation.hpp>
#include <registry/service_charge_query.hpp>
#include <set>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/text_light.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace finance {
namespace pg = userver::storages::postgres;

ServiceChargesComponent::ServiceChargesComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-finance").GetCluster()),
      exchange_rates_(context.FindComponent<ExchangeRates>()),
      invoices_(context.FindComponent<InvoicesComponent>()) {}

userver::yaml_config::Schema ServiceChargesComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::ServiceCharge ServiceChargesComponent::SaveServiceCharge(const std::string& organization,
    const platform::auth::Identity& identity, const models::SaveServiceChargeRequest& request,
    const models::ServiceChargeParties& parties, const models::ConversionRate& exchange_rate,
    bool can_write_invoices) const {
    const auto& body = request;
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    // Тот же порядок блокировок, что при включении услуги: услуга, затем счёт.
    if (request.expected_revision != 0)
        transaction.Execute(sql::kLockServiceCharges, organization,
            std::vector{boost::uuids::string_generator{}(request.id)});
    const auto labels =
        transaction.Execute(sql::kGetFinancialLabel, organization, request.label_id);
    if (labels.IsEmpty())
        throw ApiError<404>{"label_not_found", "Финансовая метка не найдена"};
    const auto value = converters::ToServiceCharge(request,
        converters::Convert(labels.AsSingleRow<models::pg::FinancialLabelRaw>()), parties,
        exchange_rate);
    const auto rows = request.expected_revision == 0
        ? transaction.Execute(sql::kCreateServiceCharge, organization, value)
        : transaction.Execute(
              sql::kUpdateServiceCharge, organization, value, request.expected_revision);
    if (rows.IsEmpty())
        throw ApiError<409>{"revision_conflict", "Услуга изменена. Обновите список"};
    const auto result = converters::Convert(rows.AsSingleRow<models::pg::ServiceChargeRaw>());
    transaction.Execute(sql::kWriteServiceChargeHistory, organization, result.id, result.revision,
        identity.ActorId(), helpers::HistorySnapshot(result, identity));
    const auto assignment =
        transaction.Execute(sql::kServiceChargeInvoiceAssignment, organization, value.Id);
    if (!assignment.IsEmpty()) {
        if (!can_write_invoices)
            throw ApiError<403>{"forbidden", "Нет права изменения счетов"};

        const auto invoice_id = assignment.AsSingleRow<boost::uuids::uuid>();
        invoices_.UpdateServiceChargeSnapshot(
            transaction, organization, identity, invoice_id, value);
    }
    if (body.target_invoice_id.has_value()) {
        if (!can_write_invoices)
            throw ApiError<403>{"forbidden", "Нет права изменения счетов"};

        if (body.expected_revision != 0 || !body.target_invoice_revision.has_value())
            throw ApiError<400>{
                "invalid_invoice_target", "Создавать услугу из счёта можно только с его редакцией"};
        invoices_.IncludeServiceCharge(transaction, organization, identity, value,
            body.target_invoice_id.value(), body.target_invoice_revision.value());
    } else if (body.target_invoice_revision.has_value())
        throw ApiError<400>{"invalid_invoice_target", "Укажите счёт для новой услуги"};
    transaction.Commit();
    return result;
}

models::ServiceChargeListResult ServiceChargesComponent::ListServiceCharges(
    const std::string& organization, const models::ListServiceChargesRequest& request) const {
    const auto& scope = request.scope;
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster,
        pg::TransactionOptions{
            pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
    const auto rows = transaction.Execute(sql::kListServiceCharges, organization, scope.shipment_id,
        scope.transportation_id, request.after, request.include_archived, request.limit + 1);
    models::ServiceChargeListResult response;
    response.items = helpers::ConvertServiceCharges(rows);
    if (response.items.size() > static_cast<std::size_t>(request.limit)) {
        response.items.pop_back();
        response.next_after = response.items.back().id;
    }
    response.invoices =
        helpers::LoadServiceChargeInvoices(transaction, organization, response.items);
    if (!scope.shipment_id.has_value()) {
        transaction.Commit();
        const auto settings = helpers::LoadFinanceSettings(cluster_, organization);
        response.reporting = {
            .currency = settings.reporting_currency, .missing_rates = {}, .summary = {}};
        return response;
    }
    const auto totals = transaction.Execute(
        sql::kServiceChargeTotals, organization, scope.shipment_id, scope.transportation_id);
    for (const auto& row : totals) {
        const auto amounts = row.As<models::ServiceChargeTotalRow>(pg::kRowTag);
        if (amounts.DirectIncome)
            response.summary.own.push_back({.currency = amounts.Currency,
                .expense_minor = amounts.DirectExpense.value(),
                .income_minor = amounts.DirectIncome.value()});
        if (amounts.TransportationIncome)
            response.summary.transportations.push_back({.currency = amounts.Currency,
                .expense_minor = amounts.TransportationExpense.value(),
                .income_minor = amounts.TransportationIncome.value()});
        response.summary.combined.push_back({.currency = amounts.Currency,
            .expense_minor = amounts.Expense,
            .income_minor = amounts.Income});
    }
    const auto reporting_rows = transaction.Execute(
        sql::kReportingServiceCharges, organization, scope.shipment_id, scope.transportation_id);
    transaction.Commit();
    const auto settings = helpers::LoadFinanceSettings(cluster_, organization);
    response.reporting = BuildReporting(
        settings.reporting_currency, reporting_rows, scope.transportation_id.has_value());
    return response;
}

models::ServiceCharge ServiceChargesComponent::FindServiceCharge(
    const std::string& organization, const models::GetServiceChargeRequest& request) const {
    const auto rows = cluster_->Execute(
        pg::ClusterHostType::kMaster, sql::kGetServiceCharge, organization, request.id);
    if (rows.IsEmpty())
        throw ApiError<404>{"service_charge_not_found", "Услуга не найдена"};
    const auto charge = converters::Convert(rows.AsSingleRow<models::pg::ServiceChargeRaw>());
    return charge;
}

bool ServiceChargesComponent::ApplyServiceChargeImport(const platform::auth::Identity& identity,
    const models::ImportServiceChargesRequest& request) const {
    const auto& command = request;
    auto actor = identity;
    if (actor.Issuer != command.actor_issuer)
        throw ApiError<400>{"invalid_actor", "Некорректный провайдер пользователя"};
    const auto producer = actor.ClientId;
    actor.Subject = command.actor_subject;
    const auto document = userver::formats::json::ValueBuilder(command).ExtractValue();
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto inserted = transaction.Execute(
        sql::kRegisterServiceChargeImport, command.organization_id, producer, command.id, document);
    if (inserted.IsEmpty()) {
        const auto old = transaction.Execute(
            sql::kGetServiceChargeImport, command.organization_id, producer, command.id);
        if (old.AsSingleRow<userver::formats::json::Value>() != document)
            throw ApiError<409>{
                "import_conflict", "Ключ переноса уже использован для другого расчёта"};
        transaction.Commit();
        return true;
    }
    std::set<std::string> ids;
    for (const auto& item : command.items) {
        const auto& charge = item.charge;
        if (!ids.insert(charge.id).second || charge.expected_revision != 0 || !charge.shipment_id ||
            charge.transportation_id || charge.target_invoice_id || charge.archived ||
            charge.currency != charge.invoice_currency || charge.contract_id ||
            (charge.manual_invoice_rate_millionths &&
                *charge.manual_invoice_rate_millionths != 1000000))
            throw ApiError<400>{"invalid_import", "Некорректные услуги для переноса в поставку"};
        const auto labels =
            transaction.Execute(sql::kGetFinancialLabel, command.organization_id, charge.label_id);
        if (labels.IsEmpty())
            throw ApiError<404>{"label_not_found", "Метка не найдена в организации"};
        auto label = converters::Convert(labels.AsSingleRow<models::pg::FinancialLabelRaw>());
        // Названия, суммы и НДС берём из согласованного снимка, а не текущего справочника.
        label.name = item.label_name;
        label.archived = false;
        label.direction = models::FinancialLabelDirection::Both;
        const auto value = converters::ToServiceCharge(charge, label,
            {.CounterpartyName = item.counterparty_name, .ContractNumber = std::nullopt},
            {.Millionths = 1000000, .Date = charge.service_date, .Source = "identity"});
        const auto rows =
            transaction.Execute(sql::kCreateServiceCharge, command.organization_id, value);
        if (rows.IsEmpty())
            throw ApiError<409>{"charge_conflict", "Услуга уже существует вне переноса"};
        const auto result = converters::Convert(rows.AsSingleRow<models::pg::ServiceChargeRaw>());
        transaction.Execute(sql::kWriteServiceChargeHistory, command.organization_id, result.id,
            result.revision, actor.ActorId(), helpers::HistorySnapshot(result, actor));
    }
    transaction.Commit();
    return true;
}

models::ServiceChargeRegistryResult ServiceChargesComponent::ListServiceChargeRegistry(
    const std::string& organization, const models::ServiceChargeRegistryRequest& request) const {
    try {
        const auto& body = request;
        const auto scope = platform::registry::CursorScope(body, organization);
        const auto order = platform::registry::RegistryOrderBy(
            registry_query::service_charges::fields::kFields, body.sort);
        const bool date_sort = order.CursorType == platform::registry::CursorKeyType::Date;
        if (body.cursor.has_value()) {
            const auto& cursor = body.cursor.value();
            if (cursor.scope != scope || date_sort != cursor.date_value.has_value() ||
                date_sort == cursor.text_value.has_value())
                throw ApiError<400>{
                    "invalid_cursor", "Курсор не соответствует фильтрам или сортировке"};
        }
        const auto query = platform::registry::BuildPage(
            registry_query::service_charges::kRegistry, body, organization);
        // Транзакция ограничивает SET LOCAL; HTTP-вызовов и записи внутри нет.
        auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster,
            pg::TransactionOptions{
                pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
        transaction.Execute(sql::kRegistryPlan);
        auto rows = transaction.Execute(query.Statement, query.Parameters)
                        .AsContainer<std::vector<models::ServiceChargeRegistryRow>>(pg::kRowTag);
        const bool more = rows.size() > static_cast<std::size_t>(body.limit);
        if (more)
            rows.pop_back();
        std::optional<models::ServiceChargeRegistryCursor> cursor;
        if (more) {
            const auto& last = rows.back();
            cursor = models::ServiceChargeRegistryCursor{
                .date_value = date_sort ? last.DateKey : std::nullopt,
                .id = boost::uuids::to_string(last.Charge.Id),
                .scope = scope,
                .text_value = date_sort ? std::nullopt : std::optional{last.TextKey}};
        }
        models::ServiceChargeRegistryResult response;
        for (const auto& row : rows)
            response.items.push_back(converters::Convert(row.Charge));
        response.invoices =
            helpers::LoadServiceChargeInvoices(transaction, organization, response.items);
        response.next_cursor = cursor;
        transaction.Commit();
        return response;
    } catch (const platform::registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

models::ServiceChargeFilterValuesResult ServiceChargesComponent::GetServiceChargeFilterValues(
    const std::string& organization, models::ServiceChargeFilterValuesRequest request) const {
    try {
        models::ServiceChargeFilterValuesResult result;
        result.fields = platform::registry::DescribeFields<models::RegistryField>(
            registry_query::service_charges::fields::kFields);
        if (!request.field.has_value())
            return result;
        if (request.query.has_value())
            request.query = userver::utils::text::ToLower(request.query.value());
        const auto query = platform::registry::BuildFilterValues(
            registry_query::service_charges::kRegistry, request, organization);
        const auto rows =
            cluster_->Execute(pg::ClusterHostType::kMaster, query.Statement, query.Parameters)
                .AsContainer<std::vector<platform::registry::ValueRow>>(pg::kRowTag);
        const bool more = rows.size() > static_cast<std::size_t>(request.limit);
        for (std::size_t index = 0;
             index < rows.size() && index < static_cast<std::size_t>(request.limit); ++index)
            result.values.push_back({.label = rows[index].Label, .value = rows[index].Value});
        if (more)
            result.next_after = result.values.back().value;
        return result;
    } catch (const platform::registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

models::ReportingSummary ServiceChargesComponent::BuildReporting(
    const std::string& currency, const pg::ResultSet& rows, bool transportation_scope) const {
    struct Totals {
        helpers::WideAmount Income = 0;
        helpers::WideAmount Expense = 0;
        bool Present = false;
    } direct, transportations, combined;

    models::ReportingSummary result{.currency = currency, .missing_rates = {}, .summary = {}};
    using Date = userver::utils::datetime::Date;
    std::map<std::pair<Date, std::string>, std::optional<models::ConversionRate>> rates;
    std::set<std::pair<Date, std::string>> missing;
    for (const auto& row : rows) {
        const auto charge = row.As<models::ReportingChargeRow>(pg::kRowTag);
        helpers::WideAmount amount;
        if (currency == charge.InvoiceCurrency)
            amount = charge.InvoiceTotalMinor;
        else {
            const auto key = std::pair{charge.Date, charge.Currency};
            if (!rates.contains(key)) {
                try {
                    rates.emplace(key, exchange_rates_.Get(charge.Currency, currency, charge.Date));
                } catch (const ApiError<503>&) {
                    rates.emplace(key, std::nullopt);
                }
            }
            if (!rates.at(key).has_value()) {
                missing.insert(key);
                continue;
            }
            amount = helpers::ConvertAmount(
                helpers::WideAmount{charge.TotalMinor}, rates.at(key).value().Millionths);
        }
        auto& group =
            charge.TransportationId.has_value() && !transportation_scope ? transportations : direct;
        group.Present = combined.Present = true;
        if (models::ParseEnumScalar(charge.Direction, models::ServiceChargeDirection{}) ==
            models::ServiceChargeDirection::Income) {
            group.Income += amount;
            combined.Income += amount;
        } else {
            group.Expense += amount;
            combined.Expense += amount;
        }
    }
    for (const auto& [date, from] : missing)
        result.missing_rates.push_back({.currency = from, .date = date});
    const auto append = [&](auto& target, const Totals& totals) {
        if (totals.Present)
            target.push_back({.currency = currency,
                .expense_minor = totals.Expense.str(),
                .income_minor = totals.Income.str()});
    };
    append(result.summary.own, direct);
    append(result.summary.transportations, transportations);
    append(result.summary.combined, combined);
    return result;
}
}  // namespace finance
