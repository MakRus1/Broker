#pragma once
#include <helpers/converters/invoices.hpp>

namespace web_bff {
inline models::finance::PaymentAllocation Convert(
    const clients::finance::models::PaymentAllocation& value) {
    models::finance::PaymentAllocation result;
    result.invoice_id = value.invoice_id;
    result.amount_minor = value.amount_minor;
    return result;
}

inline std::vector<models::finance::PaymentAllocation> Convert(
    const std::vector<clients::finance::models::PaymentAllocation>& values) {
    std::vector<models::finance::PaymentAllocation> result;
    result.reserve(values.size());
    for (const auto& value : values)
        result.push_back(Convert(value));
    return result;
}

inline clients::finance::models::PaymentAllocation Convert(
    const models::finance::PaymentAllocation& value) {
    clients::finance::models::PaymentAllocation result;
    result.invoice_id = value.invoice_id;
    result.amount_minor = value.amount_minor;
    return result;
}

inline std::vector<clients::finance::models::PaymentAllocation> Convert(
    const std::vector<models::finance::PaymentAllocation>& values) {
    std::vector<clients::finance::models::PaymentAllocation> result;
    result.reserve(values.size());
    for (const auto& value : values)
        result.push_back(Convert(value));
    return result;
}

inline models::finance::Payment Convert(const clients::finance::models::Payment& value) {
    models::finance::Payment result;
    result.id = value.id;
    result.number = value.number;
    result.own_company_id = value.own_company_id;
    result.own_company_name = value.own_company_name;
    result.counterparty_id = value.counterparty_id;
    result.counterparty_name = value.counterparty_name;
    result.own_bank_account_id = value.own_bank_account_id;
    result.counterparty_bank_account_id = value.counterparty_bank_account_id;
    result.currency = value.currency;
    result.note = value.note;
    result.archived = value.archived;
    result.revision = value.revision;
    result.kind = models::finance::ParseEnumScalar(
        clients::finance::models::EnumValue(value.kind), models::finance::PaymentKind{});
    result.payment_date = value.payment_date;
    result.amount_minor = value.amount_minor;
    result.purpose = value.purpose;
    return result;
}

inline models::finance::InvoicePayment Convert(
    const clients::finance::models::InvoicePayment& value) {
    return {.amount_minor = value.amount_minor, .payment = Convert(value.payment)};
}

inline models::finance::PaymentAvailable Convert(
    const clients::finance::models::PaymentAvailable& value) {
    return {.payment = Convert(value.payment), .unallocated_minor = value.unallocated_minor};
}

inline models::finance::PaymentInvoice Convert(
    const clients::finance::models::PaymentInvoice& value) {
    models::finance::PaymentInvoice result;
    result.invoice = Convert(value.invoice);
    result.paid_minor = value.paid_minor;
    for (const auto& payment : value.payments)
        result.payments.push_back(Convert(payment));
    result.outstanding_minor = value.outstanding_minor;
    return result;
}

inline models::finance::PaymentDetails Convert(
    const clients::finance::models::PaymentDetails& value) {
    models::finance::PaymentDetails result;
    result.payment = Convert(value.payment);
    result.allocations = Convert(value.allocations);
    result.allocated_minor = value.allocated_minor;
    result.unallocated_minor = value.unallocated_minor;
    return result;
}

inline clients::finance::ApiV1PaymentsSave::RequestBody Convert(
    const models::finance::SavePaymentRequest& value) {
    clients::finance::ApiV1PaymentsSave::RequestBody result;
    result.id = value.id;
    result.number = value.number;
    result.own_company_id = value.own_company_id;
    result.counterparty_id = value.counterparty_id;
    result.own_bank_account_id = value.own_bank_account_id;
    result.counterparty_bank_account_id = value.counterparty_bank_account_id;
    result.currency = value.currency;
    result.note = value.note;
    result.archived = value.archived;
    result.kind = clients::finance::models::ParseEnumScalar(
        models::finance::EnumValue(value.kind), clients::finance::models::PaymentKind{});
    result.payment_date = value.payment_date;
    result.amount_minor = value.amount_minor;
    result.purpose = value.purpose;
    result.expected_revision = value.expected_revision;
    result.allocations = Convert(value.allocations);
    return result;
}

inline clients::finance::ApiV1PaymentsInvoiceCandidates::RequestBody Convert(
    const models::finance::PaymentInvoiceCandidatesRequest& value) {
    clients::finance::ApiV1PaymentsInvoiceCandidates::RequestBody result;
    result.kind = clients::finance::models::ParseEnumScalar(
        models::finance::EnumValue(value.kind), clients::finance::models::PaymentKind{});
    result.own_company_id = value.own_company_id;
    result.counterparty_id = value.counterparty_id;
    result.currency = value.currency;
    result.payment_id = value.payment_id;
    result.query = value.query;
    result.after = value.after;
    result.limit = value.limit;
    return result;
}

inline clients::finance::models::ListPaymentsRequest ConvertRegistry(
    const models::finance::ListPaymentsRequest& value) {
    clients::finance::models::ListPaymentsRequest result;
    if (value.cursor.has_value())
        result.cursor = clients::finance::models::InvoiceRegistryCursor{
            .date_value = value.cursor.value().date_value,
            .id = value.cursor.value().id,
            .scope = value.cursor.value().scope,
            .text_value = value.cursor.value().text_value};
    result.descending = value.descending;
    result.filters =
        ConvertRegistryFilters<clients::finance::models::RegistryFilter>(value.filters);
    result.limit = value.limit;
    result.sort = value.sort;
    result.kind = clients::finance::models::ParseEnumScalar(
        models::finance::EnumValue(value.kind.value_or(models::finance::PaymentKind::Receipt)),
        clients::finance::models::PaymentKind{});
    return result;
}

inline clients::finance::ApiV1PaymentsList::RequestBody Convert(
    const models::finance::ListPaymentsRequest& value) {
    const auto query = ConvertRegistry(value);
    return {.cursor = query.cursor,
        .descending = query.descending,
        .filters = query.filters,
        .kind = query.kind,
        .limit = query.limit,
        .sort = query.sort};
}

inline clients::finance::ApiV1PaymentsFilterValues::RequestBody Convert(
    const models::finance::PaymentFilterValuesRequest& value) {
    clients::finance::ApiV1PaymentsFilterValues::RequestBody result;
    result.registry = ConvertRegistry(value.registry);
    result.field = value.field;
    result.query = value.query;
    result.after = value.after;
    result.limit = value.limit;
    result.candidates = value.candidates;
    return result;
}
}  // namespace web_bff
