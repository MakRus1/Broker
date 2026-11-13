#pragma once
#include <converters/service_charge.hpp>
#include <models/invoice_registry_row.hpp>
#include <models/pg/invoice_raw.hpp>

namespace finance::converters {
inline std::optional<boost::uuids::uuid> InvoiceId(const std::optional<std::string>& value) {
    return value.has_value() ? std::optional{boost::uuids::string_generator{}(value.value())}
                             : std::nullopt;
}

inline std::optional<std::string> InvoiceId(const std::optional<boost::uuids::uuid>& value) {
    return value.has_value() ? std::optional{boost::uuids::to_string(value.value())} : std::nullopt;
}

inline models::Invoice Convert(const models::pg::InvoiceRaw& value) {
    models::Invoice result;
    result.id = boost::uuids::to_string(value.Id);
    result.direction = models::ParseEnumScalar(value.Direction, models::InvoiceDirection{});
    result.number = value.Number;
    result.invoice_date = value.InvoiceDate;
    result.due_date = value.DueDate;
    result.own_company_id = boost::uuids::to_string(value.OwnCompanyId);
    result.own_company_name = value.OwnCompanyName;
    result.counterparty_id = boost::uuids::to_string(value.CounterpartyId);
    result.counterparty_name = value.CounterpartyName;
    result.own_bank_account_id = InvoiceId(value.OwnBankAccountId);
    result.counterparty_bank_account_id = InvoiceId(value.CounterpartyBankAccountId);
    result.contract_id = InvoiceId(value.ContractId);
    result.contract_number = value.ContractNumber;
    result.currency = value.Currency;
    result.note = value.Note;
    result.archived = value.Archived;
    result.revision = value.Revision;
    result.net_minor = value.NetMinor;
    result.vat_minor = value.VatMinor;
    result.total_minor = value.TotalMinor;
    return result;
}

inline std::vector<models::InvoiceRegistryItem> Convert(
    const std::vector<models::InvoiceRegistryRow>& rows) {
    std::vector<models::InvoiceRegistryItem> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
        result.push_back({.invoice = Convert(row.Invoice),
            .outstanding_minor = row.OutstandingMinor,
            .overdue_days = row.OverdueDays,
            .paid_minor = row.PaidMinor,
            .payment_status =
                models::ParseEnumScalar(row.PaymentStatus, models::InvoicePaymentStatus{})});
    return result;
}
}  // namespace finance::converters
