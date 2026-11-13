#pragma once
#include <converters/invoice.hpp>
#include <helpers/document_parties.hpp>
#include <limits>
#include <models/pg/payment_raw.hpp>
#include <platform/date_validation.hpp>
#include <userver/utils/text_light.hpp>

namespace finance::converters {
using namespace helpers;

inline models::pg::PaymentRaw ToPayment(const models::SavePaymentRequest& body) {
    if (body.expected_revision == std::numeric_limits<std::int64_t>::max())
        throw ApiError<400>{"invalid_revision", "Некорректная редакция платежа"};
    if (!platform::IsCalendarDateInRange(
            body.payment_date, std::chrono::year{1900} / 1 / 1, std::chrono::year{9999} / 12 / 31))
        throw ApiError<400>{"invalid_date", "Дата должна быть не раньше 01.01.1900"};
    if (SameId(body.own_company_id, body.counterparty_id))
        throw ApiError<400>{"same_parties", "Выберите разные стороны платежа"};
    models::pg::PaymentRaw value{.Id = boost::uuids::string_generator{}(body.id),
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
    return value;
}
}  // namespace finance::converters
