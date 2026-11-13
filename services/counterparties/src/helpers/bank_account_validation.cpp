#include <algorithm>
#include <cctype>
#include <helpers/bank_account_validation.hpp>
#include <helpers/normalization.hpp>
#include <models/errors.hpp>

namespace counterparties::helpers {
void NormalizeAndValidateBankAccount(models::BankAccount& value) {
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
}
}  // namespace counterparties::helpers
