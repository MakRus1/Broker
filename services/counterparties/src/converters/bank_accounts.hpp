#pragma once
#include <boost/uuid/string_generator.hpp>
#include <openapi/models.hpp>
#include <postgresql/types.hpp>

namespace counterparties::converters {
inline models::BankAccount Convert(const pgtypes::BankAccountV1& row) {
    return {.archived = row.Archived,
        .bank_name = row.BankName,
        .bic = row.Bic,
        .correspondent_account = row.CorrespondentAccount,
        .counterparty_id = boost::uuids::to_string(row.CounterpartyId),
        .currency = row.Currency,
        .iban = row.Iban,
        .id = boost::uuids::to_string(row.Id),
        .is_primary = row.IsPrimary,
        .name = row.Name,
        .number = row.Number,
        .revision = row.Revision,
        .swift = row.Swift};
}

inline pgtypes::BankAccountV1 Convert(const models::BankAccount& value) {
    return {.Id = boost::uuids::string_generator{}(value.id),
        .CounterpartyId = boost::uuids::string_generator{}(value.counterparty_id),
        .Name = value.name,
        .Number = value.number,
        .Currency = value.currency,
        .BankName = value.bank_name,
        .Bic = value.bic.value_or(std::nullopt),
        .Swift = value.swift.value_or(std::nullopt),
        .CorrespondentAccount = value.correspondent_account.value_or(std::nullopt),
        .Iban = value.iban.value_or(std::nullopt),
        .IsPrimary = value.is_primary,
        .Archived = value.archived,
        .Revision = value.revision};
}

inline models::BankAccount Convert(const models::SaveBankAccountRequest& input) {
    models::BankAccount value;
    value.id = boost::uuids::to_string(boost::uuids::string_generator{}(input.id));
    value.counterparty_id =
        boost::uuids::to_string(boost::uuids::string_generator{}(input.counterparty_id));
    value.name = input.name;
    value.number = input.number;
    value.currency = input.currency;
    value.bank_name = input.bank_name;
    value.bic = input.bic;
    value.swift = input.swift;
    value.correspondent_account = input.correspondent_account;
    value.iban = input.iban;
    value.is_primary = input.is_primary;
    value.archived = input.archived;
    value.revision = input.expected_revision + 1;
    return value;
}
}  // namespace counterparties::converters
