#pragma once
#include <converters/invoice.hpp>
#include <models/payment_registry_row.hpp>
#include <models/pg/payment_raw.hpp>

namespace finance::converters {
inline models::Payment Convert(const models::pg::PaymentRaw& value) {
    models::Payment result;
    result.id = boost::uuids::to_string(value.Id);
    result.kind = models::ParseEnumScalar(value.Kind, models::PaymentKind{});
    result.number = value.Number;
    result.payment_date = value.PaymentDate;
    result.own_company_id = boost::uuids::to_string(value.OwnCompanyId);
    result.own_company_name = value.OwnCompanyName;
    result.counterparty_id = boost::uuids::to_string(value.CounterpartyId);
    result.counterparty_name = value.CounterpartyName;
    result.own_bank_account_id = InvoiceId(value.OwnBankAccountId);
    result.counterparty_bank_account_id = InvoiceId(value.CounterpartyBankAccountId);
    result.currency = value.Currency;
    result.amount_minor = value.AmountMinor;
    result.purpose = value.Purpose;
    result.note = value.Note;
    result.archived = value.Archived;
    result.revision = value.Revision;
    return result;
}

inline std::vector<models::Payment> Convert(const std::vector<models::PaymentRegistryRow>& rows) {
    std::vector<models::Payment> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
        result.push_back(Convert(row.Payment));
    return result;
}
}  // namespace finance::converters
