#pragma once
#include <converters/invoice.hpp>
#include <helpers/document_parties.hpp>
#include <limits>
#include <models/errors.hpp>
#include <models/pg/invoice_raw.hpp>
#include <platform/date_validation.hpp>
#include <userver/utils/text_light.hpp>

namespace finance::converters {

inline models::pg::InvoiceRaw ToInvoice(const models::SaveInvoiceRequest& body) {
    if (body.expected_revision == std::numeric_limits<std::int64_t>::max())
        throw ApiError<400>{"invalid_revision", "Некорректная редакция счёта"};
    const auto number = userver::utils::text::Trim(body.number.value_or(""));
    if (number.empty() && (body.number.has_value() || body.expected_revision != 0))
        throw ApiError<400>{"empty_number", "Укажите номер счёта"};
    for (const auto& date : {std::optional{body.invoice_date}, body.due_date})
        if (date.has_value() &&
            !platform::IsCalendarDateInRange(
                date.value(), std::chrono::year{1900} / 1 / 1, std::chrono::year{9999} / 12 / 31))
            throw ApiError<400>{"invalid_date", "Дата должна быть не раньше 01.01.1900"};
    if (helpers::SameId(body.own_company_id, body.counterparty_id))
        throw ApiError<400>{"same_parties", "Выберите разные стороны счёта"};
    auto result = models::pg::InvoiceRaw{.Id = boost::uuids::string_generator{}(body.id),
        .Direction = models::EnumValue(body.direction),
        .Number = number,
        .InvoiceDate = body.invoice_date,
        .DueDate = body.due_date,
        .OwnCompanyId = boost::uuids::string_generator{}(body.own_company_id),
        .OwnCompanyName = {},
        .CounterpartyId = boost::uuids::string_generator{}(body.counterparty_id),
        .CounterpartyName = {},
        .OwnBankAccountId = InvoiceId(body.own_bank_account_id),
        .CounterpartyBankAccountId = InvoiceId(body.counterparty_bank_account_id),
        .ContractId = InvoiceId(body.contract_id),
        .ContractNumber = std::nullopt,
        .Currency = body.currency,
        .Note = body.note,
        .Archived = body.archived,
        .Revision = body.expected_revision + 1,
        .NetMinor = 0,
        .VatMinor = 0,
        .TotalMinor = 0};
    if (result.Note.has_value()) {
        result.Note = userver::utils::text::Trim(result.Note.value());
        if (result.Note.value().empty())
            result.Note.reset();
    }
    return result;
}
}  // namespace finance::converters
