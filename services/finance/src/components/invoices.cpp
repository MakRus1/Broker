#include <algorithm>
#include <components/invoices.hpp>
#include <converters/invoice.hpp>
#include <finance/sql_queries.hpp>
#include <helpers/document_parties.hpp>
#include <helpers/finance_settings.hpp>
#include <helpers/history/snapshot.hpp>
#include <helpers/service_charges.hpp>
#include <limits>
#include <models/errors.hpp>
#include <models/pg/invoice_raw.hpp>
#include <models/pg/service_charge_raw.hpp>
#include <platform/auth/identity.hpp>
#include <platform/document_number.hpp>
#include <registry/invoice_query.hpp>
#include <set>
#include <span>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace {
namespace helpers = finance::helpers;
namespace converters = finance::converters;
namespace models = finance::models;
namespace pg = userver::storages::postgres;
namespace sql = finance::sql;
using finance::ApiError;

void RecalculateAmounts(
    models::pg::InvoiceRaw& invoice, std::span<const models::pg::ServiceChargeRaw> charges) {
    invoice.NetMinor = 0;
    invoice.VatMinor = 0;
    invoice.TotalMinor = 0;
    for (const auto& charge : charges) {
        invoice.NetMinor += charge.InvoiceNetMinor;
        invoice.VatMinor += charge.InvoiceVatMinor;
        invoice.TotalMinor += charge.InvoiceTotalMinor;
    }
}

models::ServiceChargeDirection ServiceChargeDirection(models::InvoiceDirection direction) {
    switch (direction) {
    case models::InvoiceDirection::Outgoing:
        return models::ServiceChargeDirection::Income;
    case models::InvoiceDirection::Incoming:
        return models::ServiceChargeDirection::Expense;
    }
    throw std::logic_error("Неизвестное направление счёта");
}

std::vector<boost::uuids::uuid> InvoiceServiceChargeIds(const models::SaveInvoiceRequest& body) {
    std::set<boost::uuids::uuid> unique;
    for (const auto& id : body.service_charge_ids)
        unique.insert(boost::uuids::string_generator{}(id));
    if (unique.size() != body.service_charge_ids.size())
        throw ApiError<400>{"duplicate_service_charge", "Услуги не должны повторяться"};
    return {unique.begin(), unique.end()};
}

std::string GenerateInvoiceNumber(pg::Transaction& transaction, const std::string& organization,
    const models::pg::InvoiceRaw& invoice) {
    const auto settings =
        helpers::ReadFinanceSettings(transaction.Execute(sql::kGetFinanceSettings, organization));
    const auto direction = models::ParseEnumScalar(invoice.Direction, models::InvoiceDirection{});
    const auto& mask = direction == models::InvoiceDirection::Outgoing
        ? settings.outgoing_number_template
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
namespace pg = userver::storages::postgres;

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
    const auto details = FindInvoice(read, organization, body.id);
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
    }
    RecalculateAmounts(value, snapshots);

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
    auto result = FindInvoice(transaction, organization, request.id);
    transaction.Execute(sql::kWriteInvoiceHistory, organization, value.Id, value.Revision,
        identity.ActorId(), helpers::HistorySnapshot(result, identity));
    transaction.Commit();
    return result;
}

models::InvoiceDetails InvoicesComponent::FindInvoice(
    const std::string& organization, const models::GetInvoiceRequest& request) const {
    auto transaction = cluster_->Begin(pg::ClusterHostType::kSlave,
        pg::TransactionOptions{
            pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
    auto result = FindInvoice(transaction, organization, request.id);
    transaction.Commit();
    return result;
}

models::InvoiceListResult InvoicesComponent::ListInvoices(
    const std::string& organization, const models::ListInvoicesRequest& request) const {
    try {
        const auto& body = request;
        const auto scope = platform::registry::CursorScope(body, organization);
        const auto order = platform::registry::RegistryOrderBy(
            registry_query::invoices::registry_fields::kFields, body.sort);
        const bool date_sort = order.CursorType == platform::registry::CursorKeyType::Date;
        if (body.cursor.has_value()) {
            const auto& cursor = body.cursor.value();
            if (cursor.scope != scope || date_sort != cursor.date_value.has_value() ||
                date_sort == cursor.text_value.has_value())
                throw ApiError<400>{
                    "invalid_cursor", "Курсор не соответствует фильтрам или сортировке"};
        }
        const auto query =
            platform::registry::BuildPage(registry_query::invoices::kRegistry, body, organization);
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
        organization, models::EnumValue(ServiceChargeDirection(body.direction)),
        body.counterparty_id, body.currency, converters::InvoiceId(body.contract_id),
        converters::InvoiceId(body.invoice_id), converters::InvoiceId(body.after),
        static_cast<std::int32_t>(body.limit + 1));
    auto items = helpers::ConvertServiceCharges(rows);
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
    auto result = IncludeServiceCharge(transaction, organization, identity, charge,
        request.invoice_id, request.expected_revision, request.previous_invoice_id);
    transaction.Commit();
    return result;
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
    auto result = FindInvoice(transaction, organization, request.invoice_id);
    transaction.Commit();
    return result;
}

models::InvoiceFilterValuesResult InvoicesComponent::GetInvoiceFilterValues(
    const std::string& organization, const models::InvoiceFilterValuesRequest& request) const {
    try {
        models::InvoiceFilterValuesResult result;
        result.fields = platform::registry::DescribeFields<models::RegistryField>(
            registry_query::invoices::registry_fields::kFields);
        if (!request.field.has_value())
            return result;
        const auto query = platform::registry::BuildFilterValues(
            registry_query::invoices::kRegistry, request, organization);
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

void InvoicesComponent::ValidateInvoiceServiceCharge(
    const models::pg::ServiceChargeRaw& charge, const models::pg::InvoiceRaw& invoice) const {
    const auto direction = models::ParseEnumScalar(invoice.Direction, models::InvoiceDirection{});
    const auto charge_direction =
        models::ParseEnumScalar(charge.Direction, models::ServiceChargeDirection{});
    if (charge.Archived || charge_direction != ServiceChargeDirection(direction) ||
        charge.CounterpartyId != invoice.CounterpartyId ||
        charge.InvoiceCurrency != invoice.Currency || charge.ContractId != invoice.ContractId)
        throw ApiError<400>{"wrong_service_charge",
            "Услуга не соответствует сторонам, договору, валюте или направлению счёта"};
}

void InvoicesComponent::ValidateInvoiceSettlement(pg::Transaction& transaction,
    const std::string& organization, const models::pg::InvoiceRaw& invoice) const {
    const auto paid = transaction.Execute(sql::kInvoicePaid, organization, invoice.Id)
                          .AsSingleRow<std::int64_t>();
    if (paid > 0)
        throw ApiError<409>{"invoice_paid_readonly",
            "Счёт частично или полностью оплачен. Сначала снимите распределение оплаты в платеже"};
}

models::InvoiceDetails InvoicesComponent::FindInvoice(
    pg::Transaction& transaction, const std::string& organization, const std::string& id) const {
    const auto rows = transaction.Execute(sql::kGetInvoice, organization, id);
    if (rows.IsEmpty())
        throw ApiError<404>{"invoice_not_found", "Счёт не найден"};
    return {.charges = helpers::ConvertServiceCharges(
                transaction.Execute(sql::kGetInvoiceServiceCharges, organization, id)),
        .invoice = converters::Convert(rows.AsSingleRow<models::pg::InvoiceRaw>())};
}

void InvoicesComponent::RemoveInvoiceServiceCharge(pg::Transaction& transaction,
    const std::string& organization, const boost::uuids::uuid& invoice_id,
    const boost::uuids::uuid& service_charge_id, const platform::auth::Identity& identity) const {
    auto invoice = transaction.Execute(sql::kLockInvoices, organization, std::vector{invoice_id})
                       .AsSingleRow<models::pg::InvoiceRaw>();
    if (invoice.Archived || invoice.Revision == std::numeric_limits<std::int64_t>::max())
        throw ApiError<409>{"invoice_unavailable", "Счёт недоступен для изменения"};
    auto charges = transaction.Execute(sql::kGetInvoiceServiceCharges, organization, invoice_id)
                       .AsContainer<std::vector<models::pg::ServiceChargeRaw>>();
    std::erase_if(charges, [&](const auto& charge) { return charge.Id == service_charge_id; });
    RecalculateAmounts(invoice, charges);

    transaction.Execute(sql::kDeleteInvoiceServiceCharges, organization, invoice_id,
        std::optional{service_charge_id});
    const auto revision = invoice.Revision++;
    ValidateInvoiceSettlement(transaction, organization, invoice);
    transaction.Execute(sql::kUpdateInvoice, organization, invoice, revision);
    const auto details =
        FindInvoice(transaction, organization, boost::uuids::to_string(invoice_id));
    transaction.Execute(sql::kWriteInvoiceHistory, organization, invoice_id, invoice.Revision,
        identity.ActorId(), helpers::HistorySnapshot(details, identity));
}

void InvoicesComponent::CheckInvoiceAssignment(pg::Transaction& transaction,
    const std::string& organization, const boost::uuids::uuid& service_charge_id,
    const std::optional<std::string>& previous) const {
    const auto assignment =
        transaction.Execute(sql::kServiceChargeInvoiceAssignment, organization, service_charge_id);
    if (previous.has_value()) {
        if (assignment.IsEmpty() ||
            assignment.AsSingleRow<boost::uuids::uuid>() !=
                boost::uuids::string_generator{}(previous.value()))
            throw ApiError<409>{
                "assignment_changed", "Привязка услуги изменилась. Обновите список"};
    } else if (!assignment.IsEmpty())
        throw ApiError<409>{"service_charge_invoiced", "Услуга уже включена в счёт"};
}

models::InvoiceDetails InvoicesComponent::IncludeServiceCharge(pg::Transaction& transaction,
    const std::string& organization, const platform::auth::Identity& identity,
    const models::pg::ServiceChargeRaw& charge, const std::string& invoice_id,
    std::int64_t expected_revision, const std::optional<std::string>& previous_invoice_id) const {
    CheckInvoiceAssignment(transaction, organization, charge.Id, previous_invoice_id);
    std::vector<boost::uuids::uuid> invoice_ids{boost::uuids::string_generator{}(invoice_id)};
    if (previous_invoice_id.has_value())
        invoice_ids.push_back(boost::uuids::string_generator{}(previous_invoice_id.value()));
    transaction.Execute(sql::kLockInvoices, organization, invoice_ids);
    const auto invoices = transaction.Execute(sql::kGetInvoice, organization, invoice_id);
    if (invoices.IsEmpty())
        throw ApiError<404>{"invoice_not_found", "Счёт не найден"};
    auto value = invoices.AsSingleRow<models::pg::InvoiceRaw>();
    if (value.Archived)
        throw ApiError<409>{"archived_invoice", "Выберите действующий счёт"};
    if (value.Revision != expected_revision ||
        value.Revision == std::numeric_limits<std::int64_t>::max())
        throw ApiError<409>{"revision_conflict", "Счёт изменён. Обновите список"};
    auto charges = transaction.Execute(sql::kGetInvoiceServiceCharges, organization, value.Id)
                       .AsContainer<std::vector<models::pg::ServiceChargeRaw>>();
    if (charges.size() >= 500)
        throw ApiError<400>{
            "too_many_service_charges", "В одном счёте может быть не больше 500 услуг"};
    ValidateInvoiceServiceCharge(charge, value);
    if (previous_invoice_id.has_value()) {
        if (helpers::SameId(previous_invoice_id.value(), invoice_id))
            throw ApiError<400>{"same_invoice", "Выберите другой счёт"};
        RemoveInvoiceServiceCharge(transaction, organization,
            boost::uuids::string_generator{}(previous_invoice_id.value()), charge.Id, identity);
    }
    charges.push_back(charge);
    RecalculateAmounts(value, charges);
    ++value.Revision;
    ValidateInvoiceSettlement(transaction, organization, value);
    transaction.Execute(sql::kUpdateInvoice, organization, value, expected_revision);
    transaction.Execute(
        sql::kCreateInvoiceServiceCharge, organization, value.Id, std::vector{charge}, false);
    auto result = FindInvoice(transaction, organization, invoice_id);
    transaction.Execute(sql::kWriteInvoiceHistory, organization, value.Id, value.Revision,
        identity.ActorId(), helpers::HistorySnapshot(result, identity));
    return result;
}

void InvoicesComponent::UpdateServiceChargeSnapshot(pg::Transaction& transaction,
    const std::string& organization, const platform::auth::Identity& identity,
    const boost::uuids::uuid& invoice_id,
    const models::pg::ServiceChargeRaw& updated_charge) const {
    const auto locked =
        transaction.Execute(sql::kLockInvoices, organization, std::vector{invoice_id});
    auto invoice = locked.AsSingleRow<models::pg::InvoiceRaw>();
    if (invoice.Revision == std::numeric_limits<std::int64_t>::max())
        throw ApiError<409>{"invalid_revision", "Достигнут предел редакций счёта"};
    if (updated_charge.Archived)
        throw ApiError<409>{
            "service_charge_invoiced", "Сначала исключите услугу из счёта, затем архивируйте"};
    ValidateInvoiceServiceCharge(updated_charge, invoice);
    auto charges = transaction.Execute(sql::kGetInvoiceServiceCharges, organization, invoice_id)
                       .AsContainer<std::vector<models::pg::ServiceChargeRaw>>();
    for (auto& charge : charges) {
        if (charge.Id == updated_charge.Id)
            charge = updated_charge;
    }
    RecalculateAmounts(invoice, charges);

    const auto revision = invoice.Revision++;
    transaction.Execute(sql::kUpdateInvoiceServiceCharge, organization, invoice_id,
        updated_charge.Id, updated_charge);
    ValidateInvoiceSettlement(transaction, organization, invoice);
    transaction.Execute(sql::kUpdateInvoice, organization, invoice, revision);
    const auto details =
        FindInvoice(transaction, organization, boost::uuids::to_string(invoice_id));
    transaction.Execute(sql::kWriteInvoiceHistory, organization, invoice_id, invoice.Revision,
        identity.ActorId(), helpers::HistorySnapshot(details, identity));
}
}  // namespace finance
