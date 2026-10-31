#pragma once
#include <helpers/converters/finance.hpp>

namespace web_bff {
inline models::finance::Invoice Convert(const clients::finance::models::Invoice& value) {
    models::finance::Invoice result;
    result.id = value.id;
    result.direction = models::finance::ParseEnumScalar(
        clients::finance::models::EnumValue(value.direction), models::finance::InvoiceDirection{});
    result.number = value.number;
    result.invoice_date = value.invoice_date;
    result.due_date = value.due_date;
    result.own_company_id = value.own_company_id;
    result.own_company_name = value.own_company_name;
    result.counterparty_id = value.counterparty_id;
    result.counterparty_name = value.counterparty_name;
    result.own_bank_account_id = value.own_bank_account_id;
    result.counterparty_bank_account_id = value.counterparty_bank_account_id;
    result.contract_id = value.contract_id;
    result.contract_number = value.contract_number;
    result.currency = value.currency;
    result.note = value.note;
    result.archived = value.archived;
    result.revision = value.revision;
    result.net_minor = value.net_minor;
    result.vat_minor = value.vat_minor;
    result.total_minor = value.total_minor;
    return result;
}

inline models::finance::InvoiceRegistryItem Convert(
    const clients::finance::models::InvoiceRegistryItem& value) {
    return {.invoice = Convert(value.invoice),
        .outstanding_minor = value.outstanding_minor,
        .overdue_days = value.overdue_days,
        .paid_minor = value.paid_minor,
        .payment_status = models::finance::ParseEnumScalar(
            clients::finance::models::EnumValue(value.payment_status),
            models::finance::InvoicePaymentStatus{})};
}

inline clients::finance::ApiV1InvoicesSave::RequestBody Convert(
    const models::finance::SaveInvoiceRequest& value) {
    clients::finance::ApiV1InvoicesSave::RequestBody result;
    result.id = value.id;
    result.direction = clients::finance::models::ParseEnumScalar(
        models::finance::EnumValue(value.direction), clients::finance::models::InvoiceDirection{});
    result.number = value.number;
    result.invoice_date = value.invoice_date;
    result.due_date = value.due_date;
    result.own_company_id = value.own_company_id;
    result.counterparty_id = value.counterparty_id;
    result.own_bank_account_id = value.own_bank_account_id;
    result.counterparty_bank_account_id = value.counterparty_bank_account_id;
    result.contract_id = value.contract_id;
    result.currency = value.currency;
    result.note = value.note;
    result.archived = value.archived;
    result.service_charge_ids = value.service_charge_ids;
    result.previous_invoice_id = value.previous_invoice_id;
    result.expected_revision = value.expected_revision;
    return result;
}

inline clients::finance::ApiV1InvoicesGet::RequestBody Convert(
    const models::finance::GetInvoiceRequest& value) {
    clients::finance::ApiV1InvoicesGet::RequestBody result;
    result.id = value.id;
    return result;
}

inline clients::finance::models::ListInvoicesRequest ConvertRegistry(
    const models::finance::ListInvoicesRequest& value) {
    clients::finance::models::ListInvoicesRequest result;
    result.direction = clients::finance::models::ParseEnumScalar(
        models::finance::EnumValue(value.direction), decltype(result.direction){});
    result.sort = value.sort;
    result.descending = value.descending;
    result.limit = value.limit;
    result.filters =
        ConvertRegistryFilters<clients::finance::models::RegistryFilter>(value.filters);
    if (value.cursor.has_value())
        result.cursor = clients::finance::models::InvoiceRegistryCursor{
            .date_value = value.cursor.value().date_value,
            .id = value.cursor.value().id,
            .scope = value.cursor.value().scope,
            .text_value = value.cursor.value().text_value};
    return result;
}

inline clients::finance::ApiV1InvoicesList::RequestBody Convert(
    const models::finance::ListInvoicesRequest& value) {
    const auto query = ConvertRegistry(value);
    return {.cursor = query.cursor,
        .descending = query.descending,
        .direction = query.direction,
        .filters = query.filters,
        .limit = query.limit,
        .sort = query.sort};
}

inline models::finance::InvoiceRegistryCursor Convert(
    const clients::finance::models::InvoiceRegistryCursor& value) {
    return {.date_value = value.date_value,
        .id = value.id,
        .scope = value.scope,
        .text_value = value.text_value};
}

inline std::optional<models::finance::InvoiceRegistryCursor> Convert(
    const std::optional<clients::finance::models::InvoiceRegistryCursor>& value) {
    return value.has_value() ? std::optional{Convert(value.value())} : std::nullopt;
}

inline clients::finance::ApiV1InvoicesCandidates::RequestBody Convert(
    const models::finance::ListInvoiceCandidatesRequest& value) {
    clients::finance::ApiV1InvoicesCandidates::RequestBody result;
    result.direction = clients::finance::models::ParseEnumScalar(
        models::finance::EnumValue(value.direction), clients::finance::models::InvoiceDirection{});
    result.counterparty_id = value.counterparty_id;
    result.currency = value.currency;
    result.contract_id = value.contract_id;
    result.invoice_id = value.invoice_id;
    result.after = value.after;
    result.limit = value.limit;
    return result;
}

inline models::finance::ServiceChargeInvoice Convert(
    const clients::finance::models::ServiceChargeInvoice& value) {
    return {.direction = models::finance::ParseEnumScalar(
                clients::finance::models::EnumValue(value.direction),
                models::finance::InvoiceDirection{}),
        .invoice_date = value.invoice_date,
        .invoice_id = value.invoice_id,
        .number = value.number,
        .read_only = value.read_only,
        .service_charge_id = value.service_charge_id};
}
}  // namespace web_bff

namespace web_bff {
inline models::finance::InvoicePrintParty Convert(
    const clients::finance::models::InvoicePrintParty& value) {
    return {.kpp = value.kpp,
        .legal_address = value.legal_address,
        .name = value.name,
        .registration_id = value.registration_id,
        .tax_id = value.tax_id};
}

inline models::finance::InvoicePrintBank Convert(
    const clients::finance::models::InvoicePrintBank& value) {
    return {.bic = value.bic,
        .correspondent_account = value.correspondent_account,
        .currency = value.currency,
        .iban = value.iban,
        .name = value.name,
        .number = value.number,
        .swift = value.swift};
}

}  // namespace web_bff
