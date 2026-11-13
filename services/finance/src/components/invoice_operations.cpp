#include <algorithm>
#include <components/invoice_operations.hpp>
#include <converters/invoice.hpp>
#include <finance/sql_queries.hpp>
#include <helpers/document_parties.hpp>
#include <helpers/history/snapshot.hpp>
#include <helpers/service_charges.hpp>
#include <limits>
#include <models/errors.hpp>
#include <models/pg/invoice_raw.hpp>
#include <models/pg/service_charge_raw.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace finance::invoice_operations {

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

void ValidateInvoiceServiceCharge(
    const models::pg::ServiceChargeRaw& charge, const models::pg::InvoiceRaw& invoice) {
    if (charge.Archived ||
        charge.Direction != (invoice.Direction == "outgoing" ? "income" : "expense") ||
        charge.CounterpartyId != invoice.CounterpartyId ||
        charge.InvoiceCurrency != invoice.Currency || charge.ContractId != invoice.ContractId)
        throw ApiError<400>{"wrong_service_charge",
            "Услуга не соответствует сторонам, договору, валюте или направлению счёта"};
}

void ValidateInvoiceSettlement(pg::Transaction& transaction, const std::string& organization,
    const models::pg::InvoiceRaw& invoice) {
    const auto paid = transaction.Execute(sql::kInvoicePaid, organization, invoice.Id)
                          .AsSingleRow<std::int64_t>();
    if (paid > 0)
        throw ApiError<409>{"invoice_paid_readonly",
            "Счёт частично или полностью оплачен. Сначала снимите распределение оплаты в платеже"};
}

models::InvoiceDetails LoadInvoice(
    pg::Transaction& transaction, const std::string& organization, const std::string& id) {
    const auto rows = transaction.Execute(sql::kGetInvoice, organization, id);
    if (rows.IsEmpty())
        throw ApiError<404>{"invoice_not_found", "Счёт не найден"};
    return {.charges = helpers::ConvertServiceCharges(
                transaction.Execute(sql::kGetInvoiceServiceCharges, organization, id)),
        .invoice = converters::Convert(rows.AsSingleRow<models::pg::InvoiceRaw>())};
}

void RemoveInvoiceServiceCharge(pg::Transaction& transaction, const std::string& organization,
    const boost::uuids::uuid& invoice_id, const boost::uuids::uuid& service_charge_id,
    const platform::auth::Identity& identity) {
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
        LoadInvoice(transaction, organization, boost::uuids::to_string(invoice_id));
    transaction.Execute(sql::kWriteInvoiceHistory, organization, invoice_id, invoice.Revision,
        identity.ActorId(), helpers::HistorySnapshot(details, identity));
}

void CheckInvoiceAssignment(pg::Transaction& transaction, const std::string& organization,
    const boost::uuids::uuid& service_charge_id, const std::optional<std::string>& previous) {
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

models::InvoiceDetails IncludeServiceCharge(pg::Transaction& transaction,
    const std::string& organization, const platform::auth::Identity& identity,
    const models::pg::ServiceChargeRaw& charge, const std::string& invoice_id,
    std::int64_t expected_revision, const std::optional<std::string>& previous_invoice_id) {
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
    const auto result = LoadInvoice(transaction, organization, invoice_id);
    transaction.Execute(sql::kWriteInvoiceHistory, organization, value.Id, value.Revision,
        identity.ActorId(), helpers::HistorySnapshot(result, identity));
    return result;
}
}  // namespace finance::invoice_operations
