#pragma once
#include <helpers/document_parties.hpp>
#include <helpers/history/snapshot.hpp>
#include <helpers/service_charges.hpp>
#include <platform/date_validation.hpp>
#include <set>

namespace finance::helpers {
inline std::optional<boost::uuids::uuid> InvoiceId(const std::optional<std::string>& value) {
    return value.has_value() ? std::optional{boost::uuids::string_generator{}(value.value())}
                             : std::nullopt;
}

inline std::optional<std::string> InvoiceId(const std::optional<boost::uuids::uuid>& value) {
    return value.has_value() ? std::optional{boost::uuids::to_string(value.value())} : std::nullopt;
}

inline models::Invoice Convert(const pgtypes::InvoiceV1& value) {
    models::Invoice result;
    result.id = boost::uuids::to_string(value.Id);
    result.direction = models::ParseEnumScalar(value.Direction, models::InvoiceDirection{});
    result.number = value.Number;
    result.invoice_date = value.InvoiceDate;
    result.due_date = value.DueDate;
    result.own_company_id = boost::uuids::to_string(value.OwnCompanyId);
    result.own_company_name = value.OwnCompanyName;
    result.counterparty_id = boost::uuids::to_string(value.CounterpartyId);
    result.counterparty_name = value.CounterpartyName;
    result.own_bank_account_id = InvoiceId(value.OwnBankAccountId);
    result.counterparty_bank_account_id = InvoiceId(value.CounterpartyBankAccountId);
    result.contract_id = InvoiceId(value.ContractId);
    result.contract_number = value.ContractNumber;
    result.currency = value.Currency;
    result.note = value.Note;
    result.archived = value.Archived;
    result.revision = value.Revision;
    result.net_minor = value.NetMinor;
    result.vat_minor = value.VatMinor;
    result.total_minor = value.TotalMinor;
    return result;
}

inline std::string ServiceChargeDirection(models::InvoiceDirection direction) {
    return direction == models::InvoiceDirection::Outgoing ? "income" : "expense";
}

inline pgtypes::InvoiceV1 PrepareInvoice(const Deps& deps,
    const userver::server::request::RequestContext& context,
    const models::SaveInvoiceRequest& body) {
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
    if (SameId(body.own_company_id, body.counterparty_id))
        throw ApiError<400>{"same_parties", "Выберите разные стороны счёта"};
    auto result = pgtypes::InvoiceV1{.Id = boost::uuids::string_generator{}(body.id),
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
    ValidateDocumentParties(deps, context, body, result);
    return result;
}

inline std::vector<boost::uuids::uuid> InvoiceServiceChargeIds(
    const models::SaveInvoiceRequest& body) {
    std::set<boost::uuids::uuid> unique;
    for (const auto& id : body.service_charge_ids)
        unique.insert(boost::uuids::string_generator{}(id));
    if (unique.size() != body.service_charge_ids.size())
        throw ApiError<400>{"duplicate_service_charge", "Услуги не должны повторяться"};
    return {unique.begin(), unique.end()};
}

inline void ValidateInvoiceServiceCharge(
    const pgtypes::ServiceChargeV1& charge, const pgtypes::InvoiceV1& invoice) {
    if (charge.Archived ||
        charge.Direction != (invoice.Direction == "outgoing" ? "income" : "expense") ||
        charge.CounterpartyId != invoice.CounterpartyId ||
        charge.InvoiceCurrency != invoice.Currency || charge.ContractId != invoice.ContractId)
        throw ApiError<400>{"wrong_service_charge",
            "Услуга не соответствует сторонам, договору, валюте или направлению счёта"};
}

inline void ValidateInvoiceSettlement(pg::Transaction& transaction, const std::string& organization,
    const pgtypes::InvoiceV1& invoice) {
    const auto paid = transaction.Execute(sql::kInvoicePaid, organization, invoice.Id)
                          .AsSingleRow<std::int64_t>();
    if (paid > 0)
        throw ApiError<409>{"invoice_paid_readonly",
            "Счёт частично или полностью оплачен. Сначала снимите распределение оплаты в платеже"};
}

inline models::InvoiceDetails LoadInvoice(
    pg::Transaction& transaction, const std::string& organization, const std::string& id) {
    const auto rows = transaction.Execute(sql::kGetInvoice, organization, id);
    if (rows.IsEmpty())
        throw ApiError<404>{"invoice_not_found", "Счёт не найден"};
    return {.charges = ConvertServiceCharges(
                transaction.Execute(sql::kGetInvoiceServiceCharges, organization, id)),
        .invoice = Convert(rows.AsSingleRow<pgtypes::InvoiceV1>())};
}

inline void RemoveInvoiceServiceCharge(pg::Transaction& transaction,
    const std::string& organization, const boost::uuids::uuid& invoice_id,
    const boost::uuids::uuid& service_charge_id, const platform::auth::Identity& identity) {
    auto invoice = transaction.Execute(sql::kLockInvoices, organization, std::vector{invoice_id})
                       .AsSingleRow<pgtypes::InvoiceV1>();
    if (invoice.Archived || invoice.Revision == std::numeric_limits<std::int64_t>::max())
        throw ApiError<409>{"invoice_unavailable", "Счёт недоступен для изменения"};
    const auto rows = transaction.Execute(sql::kGetInvoiceServiceCharges, organization, invoice_id);
    invoice.NetMinor = 0;
    invoice.VatMinor = 0;
    invoice.TotalMinor = 0;
    for (const auto& line : rows.AsSetOf<pgtypes::ServiceChargeV1>()) {
        if (line.Id == service_charge_id)
            continue;
        invoice.NetMinor += line.InvoiceNetMinor;
        invoice.VatMinor += line.InvoiceVatMinor;
        invoice.TotalMinor += line.InvoiceTotalMinor;
    }
    transaction.Execute(sql::kDeleteInvoiceServiceCharges, organization, invoice_id,
        std::optional{service_charge_id});
    const auto revision = invoice.Revision++;
    ValidateInvoiceSettlement(transaction, organization, invoice);
    transaction.Execute(sql::kUpdateInvoice, organization, invoice, revision);
    const auto details =
        LoadInvoice(transaction, organization, boost::uuids::to_string(invoice_id));
    transaction.Execute(sql::kWriteInvoiceHistory, organization, invoice_id, invoice.Revision,
        identity.ActorId(), HistorySnapshot(details, identity));
}

inline void CheckInvoiceAssignment(pg::Transaction& transaction, const std::string& organization,
    const boost::uuids::uuid& service_charge_id, const std::optional<std::string>& previous) {
    const auto assignment =
        transaction.Execute(sql::kServiceChargeInvoiceAssignment, organization, service_charge_id);
    if (previous.has_value()) {
        if (assignment.IsEmpty() ||
            assignment.AsSingleRow<boost::uuids::uuid>() !=
                boost::uuids::string_generator{}(previous.value()))
            throw ApiError<409>{
                "assignment_changed", "Привязка услуги изменилась. Обновите список"};
    } else if (!assignment.IsEmpty())
        throw ApiError<409>{"service_charge_invoiced", "Услуга уже включена в счёт"};
}

inline models::InvoiceDetails IncludeServiceCharge(pg::Transaction& transaction,
    const std::string& organization, const platform::auth::Identity& identity,
    const pgtypes::ServiceChargeV1& charge, const std::string& invoice_id,
    std::int64_t expected_revision,
    const std::optional<std::string>& previous_invoice_id = std::nullopt) {
    CheckInvoiceAssignment(transaction, organization, charge.Id, previous_invoice_id);
    std::vector<boost::uuids::uuid> invoice_ids{boost::uuids::string_generator{}(invoice_id)};
    if (previous_invoice_id.has_value())
        invoice_ids.push_back(boost::uuids::string_generator{}(previous_invoice_id.value()));
    transaction.Execute(sql::kLockInvoices, organization, invoice_ids);
    const auto invoices = transaction.Execute(sql::kGetInvoice, organization, invoice_id);
    if (invoices.IsEmpty())
        throw ApiError<404>{"invoice_not_found", "Счёт не найден"};
    auto value = invoices.AsSingleRow<pgtypes::InvoiceV1>();
    if (value.Archived)
        throw ApiError<409>{"archived_invoice", "Выберите действующий счёт"};
    if (value.Revision != expected_revision ||
        value.Revision == std::numeric_limits<std::int64_t>::max())
        throw ApiError<409>{"revision_conflict", "Счёт изменён. Обновите список"};
    if (transaction.Execute(sql::kGetInvoiceServiceCharges, organization, value.Id).Size() >= 500)
        throw ApiError<400>{
            "too_many_service_charges", "В одном счёте может быть не больше 500 услуг"};
    ValidateInvoiceServiceCharge(charge, value);
    if (previous_invoice_id.has_value()) {
        if (SameId(previous_invoice_id.value(), invoice_id))
            throw ApiError<400>{"same_invoice", "Выберите другой счёт"};
        RemoveInvoiceServiceCharge(transaction, organization,
            boost::uuids::string_generator{}(previous_invoice_id.value()), charge.Id, identity);
    }
    value.NetMinor += charge.InvoiceNetMinor;
    value.VatMinor += charge.InvoiceVatMinor;
    value.TotalMinor += charge.InvoiceTotalMinor;
    ++value.Revision;
    ValidateInvoiceSettlement(transaction, organization, value);
    transaction.Execute(sql::kUpdateInvoice, organization, value, expected_revision);
    transaction.Execute(
        sql::kCreateInvoiceServiceCharge, organization, value.Id, std::vector{charge}, false);
    const auto result = LoadInvoice(transaction, organization, invoice_id);
    transaction.Execute(sql::kWriteInvoiceHistory, organization, value.Id, value.Revision,
        identity.ActorId(), HistorySnapshot(result, identity));
    return result;
}
}  // namespace finance::helpers
