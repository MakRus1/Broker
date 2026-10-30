#pragma once
#include <helpers/details.hpp>
#include <limits>

namespace counterparties::helpers {
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

inline std::vector<models::BankAccount> ConvertBankAccounts(const pg::ResultSet& rows) {
    std::vector<models::BankAccount> result;
    result.reserve(rows.Size());
    for (const auto& row : rows.AsSetOf<pgtypes::BankAccountV1>())
        result.push_back(Convert(row));
    return result;
}

inline models::BankAccount Prepare(const models::SaveBankAccountRequest& input) {
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
    if (input.expected_revision == std::numeric_limits<std::int64_t>::max())
        throw ApiError<400>{"invalid_revision", "Некорректная редакция счёта"};
    value.revision = input.expected_revision + 1;
    value.name = userver::utils::text::Trim(value.name);
    value.bank_name = userver::utils::text::Trim(value.bank_name);
    value.number = userver::utils::text::Trim(value.number);
    auto uppercase = [](std::string& text) {
        std::transform(text.begin(), text.end(), text.begin(),
            [](unsigned char c) { return std::toupper(c); });
    };
    std::erase(value.number, ' ');
    uppercase(value.number);
    uppercase(value.currency);
    for (auto* field : {&value.bic, &value.swift, &value.correspondent_account, &value.iban})
        field->emplace(Normalize(field->value_or(std::nullopt)));
    if (value.swift.value().has_value())
        uppercase(value.swift.value().value());
    if (value.iban.value().has_value()) {
        auto& iban = value.iban.value().value();
        std::erase(iban, ' ');
        uppercase(iban);
    }
    if (value.name.empty() || value.number.empty() || value.bank_name.empty())
        throw ApiError<400>{"empty_bank_requisites", "Укажите название счёта, номер и банк"};
    if (value.archived && value.is_primary)
        throw ApiError<400>{"archived_primary", "Архивный счёт не может быть основным"};
    return value;
}
}  // namespace counterparties::helpers
