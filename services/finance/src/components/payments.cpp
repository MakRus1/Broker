#include <components/payments.hpp>
#include <converters/payment.hpp>
#include <finance/sql_queries.hpp>
#include <helpers/invoices.hpp>
#include <map>
#include <models/errors.hpp>
#include <models/invoice_balance_row.hpp>
#include <models/invoice_payment_row.hpp>
#include <models/payment_allocation_row.hpp>
#include <models/payment_available_row.hpp>
#include <models/pg/invoice_raw.hpp>
#include <models/pg/payment_raw.hpp>
#include <platform/auth/identity.hpp>
#include <registry/payment_query.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace {
namespace converters = finance::converters;
namespace models = finance::models;
namespace pg = userver::storages::postgres;
namespace sql = finance::sql;
using finance::ApiError;

models::InvoiceDirection PaymentInvoiceDirection(models::PaymentKind kind) {
    switch (kind) {
    case models::PaymentKind::Receipt:
        return models::InvoiceDirection::Outgoing;
    case models::PaymentKind::Disbursement:
        return models::InvoiceDirection::Incoming;
    }
    throw std::logic_error("Неизвестный вид платежа");
}

models::PaymentDetails LoadPayment(
    pg::Transaction& transaction, const std::string& organization, const std::string& id) {
    const auto rows = transaction.Execute(sql::kGetPayment, organization, id);
    if (rows.IsEmpty())
        throw ApiError<404>{"payment_not_found", "Платёж не найден"};
    models::PaymentDetails result;
    result.payment = converters::Convert(rows.AsSingleRow<models::pg::PaymentRaw>());
    for (const auto& row : transaction.Execute(sql::kGetPaymentAllocations, organization, id)) {
        const auto allocation = row.As<models::PaymentAllocationRow>(pg::kRowTag);
        result.allocations.push_back({.amount_minor = allocation.AmountMinor,
            .invoice_id = boost::uuids::to_string(allocation.InvoiceId)});
        if (!result.payment.archived)
            result.allocated_minor += allocation.AmountMinor;
    }
    result.unallocated_minor = result.payment.amount_minor - result.allocated_minor;
    return result;
}

models::PaymentInvoice LoadInvoiceBalance(
    pg::Transaction& transaction, const std::string& organization, const std::string& id) {
    const auto rows = transaction.Execute(sql::kGetInvoice, organization, id);
    if (rows.IsEmpty())
        throw ApiError<404>{"invoice_not_found", "Счёт не найден"};
    const auto invoice = converters::Convert(rows.AsSingleRow<models::pg::InvoiceRaw>());
    const auto paid =
        transaction.Execute(sql::kInvoicePaid, organization, id).AsSingleRow<std::int64_t>();
    std::vector<models::InvoicePayment> payments;
    for (const auto& row : transaction.Execute(sql::kInvoicePayments, organization, id)) {
        const auto allocation = row.As<models::InvoicePaymentRow>(pg::kRowTag);
        payments.push_back({.amount_minor = allocation.AmountMinor,
            .payment = converters::Convert(allocation.Payment)});
    }
    return {.invoice = invoice,
        .outstanding_minor = invoice.total_minor - paid,
        .paid_minor = paid,
        .payments = std::move(payments)};
}

}  // namespace

namespace finance {
namespace pg = userver::storages::postgres;

PaymentsComponent::PaymentsComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-finance").GetCluster()) {}

userver::yaml_config::Schema PaymentsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::PaymentDetails PaymentsComponent::SavePayment(const std::string& organization,
    const platform::auth::Identity& identity, const models::SavePaymentRequest& request,
    models::pg::PaymentRaw value) const {
    const auto& body = request;
    std::map<boost::uuids::uuid, std::int64_t> allocations;
    std::int64_t allocated = 0;
    for (const auto& item : body.allocations) {
        if (item.amount_minor > value.AmountMinor - allocated)
            throw ApiError<400>{"payment_overallocated", "Распределение превышает сумму платежа"};
        allocated += item.amount_minor;
        if (!allocations
                 .emplace(boost::uuids::string_generator{}(item.invoice_id), item.amount_minor)
                 .second)
            throw ApiError<400>{"duplicate_invoice", "Счёт указан несколько раз"};
    }
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    if (body.expected_revision == 0) {
        if (transaction.Execute(sql::kCreatePayment, organization, value).IsEmpty())
            throw ApiError<409>{"revision_conflict", "Платёж уже существует. Обновите карточку"};
    } else {
        if (transaction.Execute(sql::kLockPayments, organization, value.Id).IsEmpty())
            throw ApiError<404>{"payment_not_found", "Платёж не найден"};
        if (transaction.Execute(sql::kUpdatePayment, organization, value, body.expected_revision)
                .IsEmpty())
            throw ApiError<409>{"revision_conflict", "Платёж изменён. Обновите карточку"};
    }
    std::set<boost::uuids::uuid> ids;
    for (const auto& [id, amount] : allocations)
        ids.insert(id);
    for (const auto& row : transaction.Execute(sql::kGetPaymentAllocations, organization, value.Id))
        ids.insert(row[0].As<boost::uuids::uuid>());
    // Платёж блокируется первым; все счета — в едином порядке. HTTP уже завершён.
    const auto invoices = transaction.Execute(
        sql::kLockInvoices, organization, std::vector<boost::uuids::uuid>{ids.begin(), ids.end()});
    if (invoices.Size() != ids.size())
        throw ApiError<404>{"invoice_not_found", "Счёт не найден в организации"};
    for (const auto& invoice : invoices.AsSetOf<models::pg::InvoiceRaw>()) {
        const auto found = allocations.find(invoice.Id);
        if (value.Archived || found == allocations.end())
            continue;
        const auto direction = models::EnumValue(PaymentInvoiceDirection(body.kind));
        if (invoice.Archived || invoice.Direction != direction ||
            invoice.Currency != value.Currency || invoice.OwnCompanyId != value.OwnCompanyId ||
            invoice.CounterpartyId != value.CounterpartyId)
            throw ApiError<400>{"incompatible_invoice",
                "Выберите действующий счёт с теми же сторонами, валютой и направлением"};
        const auto paid =
            transaction.Execute(sql::kInvoiceOtherPayments, organization, invoice.Id, value.Id)
                .AsSingleRow<std::int64_t>();
        if (found->second > invoice.TotalMinor - paid)
            throw ApiError<409>{
                "invoice_overpaid", "Распределение превышает остаток счёта. Обновите данные"};
    }
    transaction.Execute(sql::kDeletePaymentAllocations, organization, value.Id);
    for (const auto& [id, amount] : allocations)
        transaction.Execute(sql::kCreatePaymentAllocation, organization, value.Id, id, amount);
    const auto result = LoadPayment(transaction, organization, body.id);
    transaction.Execute(sql::kWritePaymentHistory, organization, value.Id, value.Revision,
        identity.ActorId(), helpers::HistorySnapshot(result, identity));
    transaction.Commit();
    return models::PaymentDetails{.allocated_minor = result.allocated_minor,
        .allocations = result.allocations,
        .payment = result.payment,
        .unallocated_minor = result.unallocated_minor};
}

models::PaymentDetails PaymentsComponent::FindPayment(
    const std::string& organization, const models::InvoiceBalanceRequest& request) const {
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster,
        pg::TransactionOptions{
            pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
    const auto result = LoadPayment(transaction, organization, request.id);
    transaction.Commit();
    return models::PaymentDetails{.allocated_minor = result.allocated_minor,
        .allocations = result.allocations,
        .payment = result.payment,
        .unallocated_minor = result.unallocated_minor};
}

models::PaymentListResult PaymentsComponent::ListPayments(
    const std::string& organization, const models::ListPaymentsRequest& request) const {
    try {
        const auto& body = request;
        const auto scope = platform::registry::CursorScope(body, organization);
        const auto order = platform::registry::RegistryOrderBy(
            registry_query::payments::registry_fields::kFields, body.sort);
        const bool date_sort = order.CursorType == platform::registry::CursorKeyType::Date;
        if (body.cursor.has_value()) {
            const auto& cursor = body.cursor.value();
            if (cursor.scope != scope || date_sort != cursor.date_value.has_value() ||
                date_sort == cursor.text_value.has_value())
                throw ApiError<400>{
                    "invalid_cursor", "Курсор не соответствует фильтрам или сортировке"};
        }
        const auto query =
            platform::registry::BuildPage(registry_query::payments::kRegistry, body, organization);
        // Транзакция ограничивает SET LOCAL; HTTP-вызовов и записи внутри нет.
        auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster,
            pg::TransactionOptions{
                pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
        transaction.Execute(sql::kRegistryPlan);
        auto rows = transaction.Execute(query.Statement, query.Parameters)
                        .AsContainer<std::vector<models::PaymentRegistryRow>>(pg::kRowTag);
        const bool more = rows.size() > static_cast<std::size_t>(body.limit);
        if (more)
            rows.pop_back();
        std::optional<models::InvoiceRegistryCursor> cursor;
        if (more) {
            const auto& last = rows.back();
            cursor =
                models::InvoiceRegistryCursor{.date_value = date_sort ? last.DateKey : std::nullopt,
                    .id = boost::uuids::to_string(last.Payment.Id),
                    .scope = scope,
                    .text_value = date_sort ? std::nullopt : std::optional{last.TextKey}};
        }
        transaction.Commit();
        return models::PaymentListResult{.items = converters::Convert(rows), .next_cursor = cursor};
    } catch (const platform::registry::FilterError& error) {
        throw ApiError<400>{"invalid_filter", error.what()};
    }
}

models::PaymentFilterValuesResult PaymentsComponent::GetPaymentFilterValues(
    const std::string& organization, const models::PaymentFilterValuesRequest& request) const {
    try {
        models::PaymentFilterValuesResult result;
        result.fields = platform::registry::DescribeFields<models::RegistryField>(
            registry_query::payments::registry_fields::kFields);
        if (!request.field.has_value())
            return result;
        const auto query = platform::registry::BuildFilterValues(
            registry_query::payments::kRegistry, request, organization);
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

models::PaymentInvoiceCandidatesResult PaymentsComponent::ListInvoiceCandidates(
    const std::string& organization, const models::PaymentInvoiceCandidatesRequest& request) const {
    const auto& body = request;
    const auto rows =
        cluster_->Execute(pg::ClusterHostType::kMaster, sql::kPaymentInvoiceCandidates,
            organization, models::EnumValue(PaymentInvoiceDirection(body.kind)),
            body.own_company_id, body.counterparty_id, body.currency, body.query, body.after,
            body.limit + 1, body.payment_id);
    models::PaymentInvoiceCandidatesResult result;
    for (std::size_t index = 0; index < rows.Size() && index < static_cast<std::size_t>(body.limit);
         ++index) {
        const auto balance = rows[index].As<models::InvoiceBalanceRow>(pg::kRowTag);
        const auto invoice = converters::Convert(balance.Invoice);
        result.items.push_back({.invoice = invoice,
            .outstanding_minor = invoice.total_minor - balance.PaidMinor,
            .paid_minor = balance.PaidMinor,
            .payments = {}});
    }
    if (rows.Size() > static_cast<std::size_t>(body.limit))
        result.next_after = result.items.back().invoice.id;
    return result;
}

models::PaymentInvoice PaymentsComponent::GetInvoiceBalance(
    const std::string& organization, const models::InvoiceBalanceRequest& request) const {
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster,
        pg::TransactionOptions{
            pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
    const auto result = LoadInvoiceBalance(transaction, organization, request.id);
    transaction.Commit();
    return models::PaymentInvoice{.invoice = result.invoice,
        .outstanding_minor = result.outstanding_minor,
        .paid_minor = result.paid_minor,
        .payments = result.payments};
}

models::AvailablePayments PaymentsComponent::ListPaymentCandidates(
    const std::string& organization, const models::PaymentCandidatesRequest& request) const {
    const auto& body = request;
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster,
        pg::TransactionOptions{
            pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
    const auto invoice = helpers::LoadInvoice(transaction, organization, body.invoice_id).invoice;
    if (invoice.archived)
        throw ApiError<409>{"invoice_archived", "Счёт находится в архиве"};
    const auto kind = invoice.direction == models::InvoiceDirection::Outgoing
        ? models::PaymentKind::Receipt
        : models::PaymentKind::Disbursement;
    const auto rows = transaction.Execute(sql::kPaymentCandidates, organization,
        models::EnumValue(kind), invoice.own_company_id, invoice.counterparty_id, invoice.currency,
        body.query, body.after, body.limit + 1);
    models::AvailablePayments response;
    for (std::size_t index = 0; index < rows.Size() && index < static_cast<std::size_t>(body.limit);
         ++index) {
        const auto available = rows[index].As<models::PaymentAvailableRow>(pg::kRowTag);
        response.items.push_back({.payment = converters::Convert(available.Payment),
            .unallocated_minor = available.AvailableMinor});
    }
    if (rows.Size() > static_cast<std::size_t>(body.limit))
        response.next_after = response.items.back().payment.id;
    transaction.Commit();
    return response;
}

}  // namespace finance
