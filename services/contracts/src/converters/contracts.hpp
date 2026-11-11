#pragma once
#include <boost/uuid/string_generator.hpp>
#include <models/pg/contract_raw.hpp>
#include <openapi/models.hpp>

namespace contracts::converters {
inline std::optional<boost::uuids::uuid> ConvertId(const std::optional<std::string>& value) {
    if (!value.has_value())
        return std::nullopt;
    return boost::uuids::string_generator{}(value.value());
}

inline std::optional<std::string> ConvertId(const std::optional<boost::uuids::uuid>& value) {
    if (!value.has_value())
        return std::nullopt;
    return boost::uuids::to_string(value.value());
}

inline models::Contract Convert(const models::pg::ContractRaw& value) {
    models::Contract result;
    result.id = boost::uuids::to_string(value.Id);
    result.own_company_id = boost::uuids::to_string(value.OwnCompanyId);
    result.counterparty_id = boost::uuids::to_string(value.CounterpartyId);
    result.number = value.Number;
    result.contract_date = value.ContractDate;
    result.valid_until = value.ValidUntil;
    result.currency = value.Currency;
    result.own_bank_account_id = ConvertId(value.OwnBankAccountId);
    result.counterparty_bank_account_id = ConvertId(value.CounterpartyBankAccountId);
    result.payment_delay_days = value.PaymentDelayDays;
    result.note = value.Note;
    result.archived = value.Archived;
    result.revision = value.Revision;
    return result;
}

inline models::pg::ContractRaw Convert(const models::SaveContractRequest& value) {
    return {.Id = boost::uuids::string_generator{}(value.id),
        .OwnCompanyId = boost::uuids::string_generator{}(value.own_company_id),
        .CounterpartyId = boost::uuids::string_generator{}(value.counterparty_id),
        .Number = value.number.value_or(""),
        .ContractDate = value.contract_date,
        .ValidUntil = value.valid_until,
        .Currency = value.currency,
        .OwnBankAccountId = ConvertId(value.own_bank_account_id),
        .CounterpartyBankAccountId = ConvertId(value.counterparty_bank_account_id),
        .PaymentDelayDays = value.payment_delay_days,
        .Note = value.note,
        .Archived = value.archived,
        .Revision = value.expected_revision + 1};
}
}  // namespace contracts::converters
