#pragma once
#include <clients/counterparties/client.hpp>
#include <openapi/models.hpp>

namespace finance::helpers {
inline models::InvoicePrintParty ConvertPrintParty(
    const clients::counterparties::models::Counterparty& value) {
    return {.kpp = value.kpp.value_or(std::nullopt),
        .legal_address = value.legal_address,
        .name = value.legal_name.value_or(value.name),
        .registration_id = value.registration_id,
        .tax_id = value.tax_id};
}

inline models::InvoicePrintBank ConvertPrintBank(
    const clients::counterparties::models::BankAccount& value) {
    return {.bic = value.bic.value_or(std::nullopt),
        .correspondent_account = value.correspondent_account.value_or(std::nullopt),
        .currency = value.currency,
        .iban = value.iban.value_or(std::nullopt),
        .name = value.bank_name,
        .number = value.number,
        .swift = value.swift.value_or(std::nullopt)};
}
}  // namespace finance::helpers
