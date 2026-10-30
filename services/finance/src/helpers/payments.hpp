#pragma once
#include <helpers/invoices.hpp>

namespace finance::helpers {
inline models::InvoiceDirection PaymentInvoiceDirection(models::PaymentKind kind) {
    switch (kind) {
    case models::PaymentKind::Receipt:
        return models::InvoiceDirection::Outgoing;
    case models::PaymentKind::Disbursement:
        return models::InvoiceDirection::Incoming;
    }
    throw std::logic_error("Неизвестный вид платежа");
}

inline models::Payment Convert(const pgtypes::PaymentV1& value) {
    models::Payment result;
    result.id = boost::uuids::to_string(value.Id);
    result.kind = models::ParseEnumScalar(value.Kind, models::PaymentKind{});
    result.number = value.Number;
    result.payment_date = value.PaymentDate;
    result.own_company_id = boost::uuids::to_string(value.OwnCompanyId);
    result.own_company_name = value.OwnCompanyName;
    result.counterparty_id = boost::uuids::to_string(value.CounterpartyId);
    result.counterparty_name = value.CounterpartyName;
    result.own_bank_account_id = InvoiceId(value.OwnBankAccountId);
    result.counterparty_bank_account_id = InvoiceId(value.CounterpartyBankAccountId);
    result.currency = value.Currency;
    result.amount_minor = value.AmountMinor;
    result.purpose = value.Purpose;
    result.note = value.Note;
    result.archived = value.Archived;
    result.revision = value.Revision;
    return result;
}

inline pgtypes::PaymentV1 PreparePayment(const Deps& deps,
    const userver::server::request::RequestContext& context,
    const models::SavePaymentRequest& body) {
    if (body.expected_revision == std::numeric_limits<std::int64_t>::max())
        throw ApiError<400>{"invalid_revision", "Некорректная редакция платежа"};
    if (!platform::IsCalendarDateInRange(
            body.payment_date, std::chrono::year{1900} / 1 / 1, std::chrono::year{9999} / 12 / 31))
        throw ApiError<400>{"invalid_date", "Дата должна быть не раньше 01.01.1900"};
    if (SameId(body.own_company_id, body.counterparty_id))
        throw ApiError<400>{"same_parties", "Выберите разные стороны платежа"};
    pgtypes::PaymentV1 value{.Id = boost::uuids::string_generator{}(body.id),
        .Kind = models::EnumValue(body.kind),
        .Number = userver::utils::text::Trim(body.number),
        .PaymentDate = body.payment_date,
        .OwnCompanyId = boost::uuids::string_generator{}(body.own_company_id),
        .OwnCompanyName = {},
        .CounterpartyId = boost::uuids::string_generator{}(body.counterparty_id),
        .CounterpartyName = {},
        .OwnBankAccountId = InvoiceId(body.own_bank_account_id),
        .CounterpartyBankAccountId = InvoiceId(body.counterparty_bank_account_id),
        .Currency = body.currency,
        .AmountMinor = body.amount_minor,
        .Purpose = body.purpose,
        .Note = body.note,
        .Archived = body.archived,
        .Revision = body.expected_revision + 1};
    if (value.Number.empty())
        throw ApiError<400>{"empty_number", "Укажите номер платежа"};
    ValidateDocumentParties(deps, context, body, value);
    return value;
}

inline models::PaymentDetails LoadPayment(
    pg::Transaction& transaction, const std::string& organization, const std::string& id) {
    const auto rows = transaction.Execute(sql::kGetPayment, organization, id);
    if (rows.IsEmpty())
        throw ApiError<404>{"payment_not_found", "Платёж не найден"};
    models::PaymentDetails result;
    result.payment = Convert(rows.AsSingleRow<pgtypes::PaymentV1>());
    for (const auto& row : transaction.Execute(sql::kGetPaymentAllocations, organization, id)) {
        const auto [invoice, amount] = row.As<boost::uuids::uuid, std::int64_t>();
        result.allocations.push_back(
            {.amount_minor = amount, .invoice_id = boost::uuids::to_string(invoice)});
        if (!result.payment.archived)
            result.allocated_minor += amount;
    }
    result.unallocated_minor = result.payment.amount_minor - result.allocated_minor;
    return result;
}

inline models::PaymentInvoice InvoiceBalance(
    pg::Transaction& transaction, const std::string& organization, const std::string& id) {
    const auto rows = transaction.Execute(sql::kGetInvoice, organization, id);
    if (rows.IsEmpty())
        throw ApiError<404>{"invoice_not_found", "Счёт не найден"};
    const auto invoice = Convert(rows.AsSingleRow<pgtypes::InvoiceV1>());
    const auto paid =
        transaction.Execute(sql::kInvoicePaid, organization, id).AsSingleRow<std::int64_t>();
    std::vector<models::InvoicePayment> payments;
    for (const auto& row : transaction.Execute(sql::kInvoicePayments, organization, id)) {
        const auto [payment, amount] = row.As<pgtypes::PaymentV1, std::int64_t>();
        payments.push_back({.amount_minor = amount, .payment = Convert(payment)});
    }
    return {.invoice = invoice,
        .outstanding_minor = invoice.total_minor - paid,
        .paid_minor = paid,
        .payments = std::move(payments)};
}
}  // namespace finance::helpers
