#include <components/invoices.hpp>
#include <converters/invoice.hpp>
#include <finance/sql_queries.hpp>
#include <helpers/finance_settings.hpp>
#include <helpers/invoices.hpp>
#include <models/errors.hpp>
#include <models/pg/invoice_raw.hpp>
#include <models/pg/service_charge_raw.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <platform/document_number.hpp>
#include <registry/invoice_query.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace {
namespace converters = finance::converters;
namespace models = finance::models;
namespace pg = userver::storages::postgres;
namespace sql = finance::sql;
using finance::ApiError;
using namespace finance::helpers;

std::string ServiceChargeDirection(models::InvoiceDirection direction) {
    return direction == models::InvoiceDirection::Outgoing ? "income" : "expense";
}

std::vector<boost::uuids::uuid> InvoiceServiceChargeIds(const models::SaveInvoiceRequest& body) {
    std::set<boost::uuids::uuid> unique;
    for (const auto& id : body.service_charge_ids)
        unique.insert(boost::uuids::string_generator{}(id));
    if (unique.size() != body.service_charge_ids.size())
        throw ApiError<400>{"duplicate_service_charge", "Услуги не должны повторяться"};
    return {unique.begin(), unique.end()};
}
}  // namespace

namespace {
std::string GenerateInvoiceNumber(pg::Transaction& transaction, const std::string& organization,
    const models::pg::InvoiceRaw& invoice) {
    const auto settings =
        ConvertFinanceSettings(transaction.Execute(sql::kGetFinanceSettings, organization));
    const auto& mask = invoice.Direction == "outgoing" ? settings.outgoing_number_template
                                                       : settings.incoming_number_template;
    const auto period =
        platform::DocumentNumberPeriod(invoice.InvoiceDate, settings.reset_numbers_yearly);
    const auto sequence =
        transaction.Execute(sql::kNextDocumentNumber, organization, invoice.Direction, period)
            .AsSingleRow<std::int64_t>();
    const auto number = platform::FormatDocumentNumber(mask, invoice.InvoiceDate, sequence);
    if (number.size() > 100)
        throw ApiError<400>{"number_too_long", "Номер счёта длиннее 100 символов"};
    return number;
}
}  // namespace

namespace finance {
using namespace helpers;

InvoicesComponent::InvoicesComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-finance").GetCluster()) {}

userver::yaml_config::Schema InvoicesComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::InvoicePrintSnapshot InvoicesComponent::ReadInvoicePrintSnapshot(
    const std::string& organization, const models::InvoicePrintRequest& request) const {
    const auto& body = request;
    auto read = cluster_->Begin(pg::ClusterHostType::kMaster,
        pg::TransactionOptions{
            pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
    const auto details = LoadInvoice(read, organization, body.id);
    if (details.invoice.revision != body.revision)
        throw ApiError<409>{
            "revision_conflict", "Счёт изменён. Обновите карточку перед скачиванием"};
    if (details.invoice.direction != models::InvoiceDirection::Outgoing)
        throw ApiError<400>{"not_outgoing_invoice", "Печатная форма доступна для исходящего счёта"};
    const auto cached =
        read.Execute(sql::kGetInvoicePrintSnapshot, organization, body.id, body.revision);
    read.Commit();
    if (!cached.IsEmpty())
        return {.Details = details,
            .Cached = broker_codegen::ParseJson<models::InvoicePrintData>(
                cached.AsSingleRow<userver::formats::json::Value>())};

    return {.Details = details, .Cached = std::nullopt};
}

models::InvoicePrintData InvoicesComponent::SaveInvoicePrintSnapshot(
    const std::string& organization, const models::InvoicePrintData& data) const {
    auto write = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto current =
        write.Execute(sql::kLockInvoicePrintRevision, organization, data.invoice.id);
    if (current.IsEmpty() || current.AsSingleRow<std::int64_t>() != data.invoice.revision)
        throw ApiError<409>{
            "revision_conflict", "Счёт изменён. Обновите карточку перед скачиванием"};
    auto saved = write.Execute(sql::kSaveInvoicePrintSnapshot, organization, data.invoice.id,
        data.invoice.revision, userver::formats::json::ValueBuilder(data).ExtractValue());
    if (saved.IsEmpty())
        saved = write.Execute(
            sql::kGetInvoicePrintSnapshot, organization, data.invoice.id, data.invoice.revision);
    const auto result = broker_codegen::ParseJson<models::InvoicePrintData>(
        saved.AsSingleRow<userver::formats::json::Value>());
    write.Commit();
    return result;
}

models::InvoiceDetails InvoicesComponent::SaveInvoice(const std::string& organization,
    const platform::auth::Identity& identity, const models::SaveInvoiceRequest& request,
    models::pg::InvoiceRaw value) const {

    const auto ids = InvoiceServiceChargeIds(request);
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    if (value.Number.empty())
        value.Number = GenerateInvoiceNumber(transaction, organization, value);
    // Общий порядок блокировок защищает от одновременного редактирования и включения услуг в
    // счета.
    const auto charges = transaction.Execute(sql::kLockServiceCharges, organization, ids);
    if (charges.Size() != ids.size())
        throw ApiError<404>{"service_charge_not_found", "Услуга не найдена в организации"};
    if (request.previous_invoice_id.has_value()) {
        if (request.expected_revision != 0 || ids.size() != 1)
            throw ApiError<400>{
                "invalid_reassignment", "Новый счёт при перепривязке создаётся из одной услуги"};
        CheckInvoiceAssignment(transaction, organization, ids.front(), request.previous_invoice_id);
        RemoveInvoiceServiceCharge(transaction, organization,
            boost::uuids::string_generator{}(request.previous_invoice_id.value()), ids.front(),
            identity);
    }
    if (!transaction.Execute(sql::kConflictingInvoiceServiceCharges, organization, ids, value.Id)
             .IsEmpty())
        throw ApiError<409>{"service_charge_invoiced", "Услуга уже включена в другой счёт"};
    std::vector<models::pg::ServiceChargeRaw> snapshots;
    snapshots.reserve(charges.Size());
    for (const auto& charge : charges.AsSetOf<models::pg::ServiceChargeRaw>()) {
        ValidateInvoiceServiceCharge(charge, value);
        snapshots.push_back(charge);
        value.NetMinor += charge.InvoiceNetMinor;
        value.VatMinor += charge.InvoiceVatMinor;
        value.TotalMinor += charge.InvoiceTotalMinor;
    }
    if (request.expected_revision != 0)
        transaction.Execute(sql::kLockInvoices, organization, std::vector{value.Id});
    ValidateInvoiceSettlement(transaction, organization, value);
    const auto saved = request.expected_revision == 0
        ? transaction.Execute(sql::kCreateInvoice, organization, value)
        : transaction.Execute(sql::kUpdateInvoice, organization, value, request.expected_revision);
    if (saved.IsEmpty())
        throw ApiError<409>{"revision_conflict", "Счёт изменён. Обновите карточку"};
    transaction.Execute(sql::kDeleteInvoiceServiceCharges, organization, value.Id,
        std::optional<boost::uuids::uuid>{});
    transaction.Execute(
        sql::kCreateInvoiceServiceCharge, organization, value.Id, snapshots, value.Archived);
    const auto result = LoadInvoice(transaction, organization, request.id);
    transaction.Execute(sql::kWriteInvoiceHistory, organization, value.Id, value.Revision,
        identity.ActorId(), HistorySnapshot(result, identity));
    transaction.Commit();
    return models::InvoiceDetails{.charges = result.charges, .invoice = result.invoice};
}

models::InvoiceDetails InvoicesComponent::FindInvoice(
    const std::string& organization, const models::GetInvoiceRequest& request) const {

    auto transaction = cluster_->Begin(pg::ClusterHostType::kSlave,
        pg::TransactionOptions{
            pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
    const auto result = LoadInvoice(transaction, organization, request.id);
    transaction.Commit();
    return models::InvoiceDetails{.charges = result.charges, .invoice = result.invoice};
}

models::InvoiceListResult InvoicesComponent::ListInvoices(
    const std::string& organization, const models::ListInvoicesRequest& request) const {
    using namespace ::finance::registry_query::invoices;
    try {

        const auto& body = request;
        const auto scope = platform::registry::CursorScope(body, organization);
        const auto order = registry::RegistryOrderBy(registry_fields::kFields, body.sort);
        const bool date_sort = order.CursorType == registry::CursorKeyType::Date;
        if (body.cursor.has_value()) {
            const auto& cursor = body.cursor.value();
            if (cursor.scope != scope || date_sort != cursor.date_value.has_value() ||
                date_sort == cursor.text_value.has_value())
                throw ApiError<400>{
                    "invalid_cursor", "Курсор не соответствует фильтрам или сортировке"};
        }
        const auto query = platform::registry::BuildPage(kRegistry, body, organization);
        // Транзакция ограничивает SET LOCAL; HTTP-вызовов и записи внутри нет.
        auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster,
            pg::TransactionOptions{
                pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
        transaction.Execute(sql::kRegistryPlan);
        auto rows = transaction.Execute(query.Statement, query.Parameters)
                        .AsContainer<std::vector<models::InvoiceRegistryRow>>(pg::kRowTag);
        const bool more = rows.size() > static_cast<std::size_t>(body.limit);
        if (more)
            rows.pop_back();
        std::optional<models::InvoiceRegistryCursor> cursor;
        if (more) {
            const auto& last = rows.back();
            cursor =
                models::InvoiceRegistryCursor{.date_value = date_sort ? last.DateKey : std::nullopt,
                    .id = boost::uuids::to_string(last.Invoice.Id),
                    .scope = scope,
                    .text_value = date_sort ? std::nullopt : std::optional{last.TextKey}};
        }
        transaction.Commit();
        return models::InvoiceListResult{.items = converters::Convert(rows), .next_cursor = cursor};
    } catch (const platform::registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

models::InvoiceCandidatesResult InvoicesComponent::ListServiceChargeCandidates(
    const std::string& organization, const models::ListInvoiceCandidatesRequest& request) const {

    const auto& body = request;
    const auto rows = cluster_->Execute(pg::ClusterHostType::kSlave, sql::kListInvoiceCandidates,
        organization, ServiceChargeDirection(body.direction), body.counterparty_id, body.currency,
        converters::InvoiceId(body.contract_id), converters::InvoiceId(body.invoice_id),
        converters::InvoiceId(body.after), static_cast<std::int32_t>(body.limit + 1));
    auto items = ConvertServiceCharges(rows);
    std::optional<std::string> next;
    if (items.size() > static_cast<std::size_t>(body.limit)) {
        items.pop_back();
        next = items.back().id;
    }
    return models::InvoiceCandidatesResult{.items = std::move(items), .next_after = next};
}

models::InvoiceDetails InvoicesComponent::IncludeServiceCharge(const std::string& organization,
    const platform::auth::Identity& identity,
    const models::AddInvoiceServiceChargeRequest& request) const {

    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto charges = transaction.Execute(sql::kLockServiceCharges, organization,
        std::vector{boost::uuids::string_generator{}(request.service_charge_id)});
    if (charges.IsEmpty())
        throw ApiError<404>{"service_charge_not_found", "Услуга не найдена"};
    const auto charge = charges.AsSingleRow<models::pg::ServiceChargeRaw>();
    const auto result = helpers::IncludeServiceCharge(transaction, organization, identity, charge,
        request.invoice_id, request.expected_revision, request.previous_invoice_id);
    transaction.Commit();
    return models::InvoiceDetails{.charges = result.charges, .invoice = result.invoice};
}

models::InvoiceDetails InvoicesComponent::DetachServiceCharge(const std::string& organization,
    const platform::auth::Identity& identity,
    const models::DetachInvoiceServiceChargeRequest& request) const {

    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto charges = transaction.Execute(sql::kLockServiceCharges, organization,
        std::vector{boost::uuids::string_generator{}(request.service_charge_id)});
    if (charges.IsEmpty())
        throw ApiError<404>{"service_charge_not_found", "Услуга не найдена"};
    const auto charge = charges.AsSingleRow<models::pg::ServiceChargeRaw>();
    CheckInvoiceAssignment(transaction, organization, charge.Id, std::optional{request.invoice_id});
    RemoveInvoiceServiceCharge(transaction, organization,
        boost::uuids::string_generator{}(request.invoice_id), charge.Id, identity);
    const auto result = LoadInvoice(transaction, organization, request.invoice_id);
    transaction.Commit();
    return models::InvoiceDetails{.charges = result.charges, .invoice = result.invoice};
}

models::InvoiceFilterValuesResult InvoicesComponent::GetInvoiceFilterValues(
    const std::string& organization, const models::InvoiceFilterValuesRequest& request) const {
    using namespace ::finance::registry_query::invoices;
    try {
        models::InvoiceFilterValuesResult result;
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

}  // namespace finance
