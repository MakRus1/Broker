#pragma once
#include <models/pg/contract_raw.hpp>
#include <openapi/models.hpp>

namespace contracts::converters {
inline models::Contract Convert(const models::pg::ContractRaw& value) {
    models::Contract result;
    result.id = value.Id;
    result.own_company_id = value.OwnCompanyId;
    result.counterparty_id = value.CounterpartyId;
    result.number = value.Number;
    result.contract_date = value.ContractDate;
    result.valid_until = value.ValidUntil;
    result.currency = value.Currency;
    result.own_bank_account_id = value.OwnBankAccountId;
    result.counterparty_bank_account_id = value.CounterpartyBankAccountId;
    result.payment_delay_days = value.PaymentDelayDays;
    result.note = value.Note;
    result.archived = value.Archived;
    result.revision = value.Revision;
    return result;
}

inline models::pg::ContractRaw Convert(const models::SaveContractRequest& value) {
    return {.Id = value.id,
        .OwnCompanyId = value.own_company_id,
        .CounterpartyId = value.counterparty_id,
        .Number = value.number.value_or(""),
        .ContractDate = value.contract_date,
        .ValidUntil = value.valid_until,
        .Currency = value.currency,
        .OwnBankAccountId = value.own_bank_account_id,
        .CounterpartyBankAccountId = value.counterparty_bank_account_id,
        .PaymentDelayDays = value.payment_delay_days,
        .Note = value.note,
        .Archived = value.archived,
        .Revision = value.expected_revision + 1};
}
}  // namespace contracts::converters
