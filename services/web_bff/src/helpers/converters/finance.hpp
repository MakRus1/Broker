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

inline models::finance::ExchangeRate Convert(const clients::finance::models::ExchangeRate& value) {
    return {.date = value.date,
        .rate_millionths = value.rate_millionths,
        .source =
            models::finance::ParseEnumScalar(clients::finance::models::EnumValue(value.source),
                models::finance::ExchangeRate_source{})};
}

inline models::finance::ServiceCharge Convert(
    const clients::finance::models::ServiceCharge& value) {
    return {
        .archived = value.archived,
        .contract_id = value.contract_id,
        .contract_number = value.contract_number,
        .counterparty_id = value.counterparty_id,
        .counterparty_name = value.counterparty_name,
        .currency = value.currency,
        .direction =
            models::finance::ParseEnumScalar(clients::finance::models::EnumValue(value.direction),
                models::finance::ServiceChargeDirection{}),
        .id = value.id,
        .invoice_currency = value.invoice_currency,
        .invoice_net_minor = value.invoice_net_minor,
        .invoice_rate = Convert(value.invoice_rate),
        .invoice_total_minor = value.invoice_total_minor,
        .invoice_vat_minor = value.invoice_vat_minor,
        .label_id = value.label_id,
        .label_name = value.label_name,
        .net_minor = value.net_minor,
        .note = value.note,
        .price_minor = value.price_minor,
        .price_mode = models::finance::ParseEnumScalar(
            clients::finance::models::EnumValue(value.price_mode), models::finance::PriceMode{}),
        .quantity_milli = value.quantity_milli,
        .revision = value.revision,
        .service_date = value.service_date,
        .shipment_id = value.shipment_id,
        .total_minor = value.total_minor,
        .transportation_id = value.transportation_id,
        .unit = models::finance::ParseEnumScalar(
            clients::finance::models::EnumValue(value.unit), models::finance::FinancialLabelUnit{}),
        .vat_minor = value.vat_minor,
        .vat_mode = models::finance::ParseEnumScalar(
            clients::finance::models::EnumValue(value.vat_mode), models::finance::VatMode{}),
        .vat_rate_basis_points = value.vat_rate_basis_points,
    };
}

inline models::finance::CurrencyTotal Convert(
    const clients::finance::models::CurrencyTotal& value) {
    return {
        .currency = value.currency,
        .expense_minor = value.expense_minor,
        .income_minor = value.income_minor,
    };
}

inline models::finance::ServiceChargeSummary Convert(
    const clients::finance::models::ServiceChargeSummary& value) {
    models::finance::ServiceChargeSummary result;
    for (const auto& total : value.own)
        result.own.push_back(Convert(total));
    for (const auto& total : value.transportations)
        result.transportations.push_back(Convert(total));
    for (const auto& total : value.combined)
        result.combined.push_back(Convert(total));
    return result;
}

inline models::finance::ReportingSummary Convert(
    const clients::finance::models::ReportingSummary& value) {
    models::finance::ReportingSummary result{
        .currency = value.currency, .missing_rates = {}, .summary = Convert(value.summary)};
    for (const auto& missing : value.missing_rates)
        result.missing_rates.push_back({.currency = missing.currency, .date = missing.date});
    return result;
}

inline clients::finance::ApiV1ServiceChargesSave::RequestBody Convert(
    const models::finance::SaveServiceChargeRequest& value) {
    return {
        .archived = value.archived,
        .contract_id = value.contract_id,
        .counterparty_id = value.counterparty_id,
        .currency = value.currency,
        .direction =
            clients::finance::models::ParseEnumScalar(models::finance::EnumValue(value.direction),
                clients::finance::models::ServiceChargeDirection{}),
        .expected_revision = value.expected_revision,
        .id = value.id,
        .invoice_currency = value.invoice_currency,
        .label_id = value.label_id,
        .manual_invoice_rate_millionths = value.manual_invoice_rate_millionths,
        .note = value.note,
        .price_minor = value.price_minor,
        .price_mode = clients::finance::models::ParseEnumScalar(
            models::finance::EnumValue(value.price_mode), clients::finance::models::PriceMode{}),
        .quantity_milli = value.quantity_milli,
        .service_date = value.service_date,
        .shipment_id = value.shipment_id,
        .transportation_id = value.transportation_id,
        .unit = clients::finance::models::ParseEnumScalar(
            models::finance::EnumValue(value.unit), clients::finance::models::FinancialLabelUnit{}),
        .vat_mode = clients::finance::models::ParseEnumScalar(
            models::finance::EnumValue(value.vat_mode), clients::finance::models::VatMode{}),
        .vat_rate_basis_points = value.vat_rate_basis_points,
    };
}

inline clients::finance::ApiV1ServiceChargesList::RequestBody Convert(
    const models::finance::ListServiceChargesRequest& value) {
    return {
        .after = value.after,
        .include_archived = value.include_archived,
        .limit = value.limit,
        .scope =
            clients::finance::models::ServiceChargeScope{.shipment_id = value.scope.shipment_id,
                .transportation_id = value.scope.transportation_id},
    };
}
}  // namespace web_bff
