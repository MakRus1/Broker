#pragma once
#include <clients/contracts/client.hpp>
#include <helpers/registry_filters.hpp>
#include <openapi/models.hpp>

namespace web_bff {
inline models::contracts::Contract Convert(const clients::contracts::models::Contract& value) {
    return {.archived = value.archived,
        .contract_date = value.contract_date,
        .counterparty_bank_account_id = value.counterparty_bank_account_id,
        .counterparty_id = value.counterparty_id,
        .currency = value.currency,
        .id = value.id,
        .note = value.note,
        .number = value.number,
        .own_bank_account_id = value.own_bank_account_id,
        .own_company_id = value.own_company_id,
        .payment_delay_days = value.payment_delay_days,
        .revision = value.revision,
        .valid_until = value.valid_until};
}

inline clients::contracts::ApiV1ContractsSave::RequestBody Convert(
    const models::contracts::SaveContractRequest& value) {
    return {.archived = value.archived,
        .contract_date = value.contract_date,
        .counterparty_bank_account_id = value.counterparty_bank_account_id,
        .counterparty_id = value.counterparty_id,
        .currency = value.currency,
        .expected_revision = value.expected_revision,
        .id = value.id,
        .note = value.note,
        .number = value.number,
        .own_bank_account_id = value.own_bank_account_id,
        .own_company_id = value.own_company_id,
        .payment_delay_days = value.payment_delay_days,
        .valid_until = value.valid_until};
}

inline clients::contracts::models::ListContractsRequest ConvertRegistry(
    const models::contracts::ListContractsRequest& value) {
    clients::contracts::models::ListContractsRequest result;
    result.sort = value.sort;
    result.descending = value.descending;
    result.limit = value.limit;
    result.filters =
        ConvertRegistryFilters<clients::contracts::models::RegistryFilter>(value.filters);
    if (value.cursor.has_value())
        result.cursor = clients::contracts::models::ContractRegistryCursor{
            .date_value = value.cursor.value().date_value,
            .id = value.cursor.value().id,
            .scope = value.cursor.value().scope,
            .text_value = value.cursor.value().text_value};
    return result;
}

inline clients::contracts::ApiV1ContractsList::RequestBody Convert(
    const models::contracts::ListContractsRequest& value) {
    const auto query = ConvertRegistry(value);
    return {.cursor = query.cursor,
        .descending = query.descending,
        .filters = query.filters,
        .limit = query.limit,
        .sort = query.sort};
}
}  // namespace web_bff
