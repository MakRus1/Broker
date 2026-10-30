#pragma once
#include <clients/finance/client.hpp>
#include <helpers/registry_filters.hpp>
#include <openapi/models.hpp>

namespace web_bff {
inline models::finance::FinancialLabel Convert(
    const clients::finance::models::FinancialLabel& value) {
    return {.archived = value.archived,
        .currency = value.currency,
        .direction =
            models::finance::ParseEnumScalar(clients::finance::models::EnumValue(value.direction),
                models::finance::FinancialLabelDirection{}),
        .id = value.id,
        .name = value.name,
        .note = value.note,
        .revision = value.revision,
        .unit = models::finance::ParseEnumScalar(
            clients::finance::models::EnumValue(value.unit), models::finance::FinancialLabelUnit{}),
        .vat_mode = models::finance::ParseEnumScalar(
            clients::finance::models::EnumValue(value.vat_mode), models::finance::VatMode{}),
        .vat_rate_basis_points = value.vat_rate_basis_points};
}

inline clients::finance::ApiV1FinancialLabelsSave::RequestBody Convert(
    const models::finance::SaveFinancialLabelRequest& value) {
    return {.archived = value.archived,
        .currency = value.currency,
        .direction =
            clients::finance::models::ParseEnumScalar(models::finance::EnumValue(value.direction),
                clients::finance::models::FinancialLabelDirection{}),
        .expected_revision = value.expected_revision,
        .id = value.id,
        .name = value.name,
        .note = value.note,
        .unit = clients::finance::models::ParseEnumScalar(
            models::finance::EnumValue(value.unit), clients::finance::models::FinancialLabelUnit{}),
        .vat_mode = clients::finance::models::ParseEnumScalar(
            models::finance::EnumValue(value.vat_mode), clients::finance::models::VatMode{}),
        .vat_rate_basis_points = value.vat_rate_basis_points};
}

inline clients::finance::models::ListFinancialLabelsRequest ConvertRegistry(
    const models::finance::ListFinancialLabelsRequest& value) {
    clients::finance::models::ListFinancialLabelsRequest result;
    result.sort = value.sort;
    result.descending = value.descending;
    result.limit = value.limit;
    result.filters =
        ConvertRegistryFilters<clients::finance::models::RegistryFilter>(value.filters);
    if (value.cursor.has_value())
        result.cursor =
            clients::finance::models::FinancialLabelRegistryCursor{.id = value.cursor.value().id,
                .scope = value.cursor.value().scope,
                .value = value.cursor.value().value};
    return result;
}

inline clients::finance::ApiV1FinancialLabelsList::RequestBody Convert(
    const models::finance::ListFinancialLabelsRequest& value) {
    const auto query = ConvertRegistry(value);
    return {.cursor = query.cursor,
        .descending = query.descending,
        .filters = query.filters,
        .limit = query.limit,
        .sort = query.sort};
}
}  // namespace web_bff
