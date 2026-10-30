#pragma once
#include <algorithm>
#include <boost/uuid/string_generator.hpp>
#include <contracts/sql_queries.hpp>
#include <deps.hpp>
#include <limits>
#include <models/errors.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <platform/date_validation.hpp>
#include <platform/http_client.hpp>
#include <postgresql/types.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/utils/text_light.hpp>

namespace contracts::helpers {
namespace pg = userver::storages::postgres;

inline std::optional<boost::uuids::uuid> ConvertId(const std::optional<std::string>& value) {
    if (!value.has_value())
        return std::nullopt;
    return boost::uuids::string_generator{}(value.value());
}

inline std::optional<std::string> ConvertId(const std::optional<boost::uuids::uuid>& value) {
    if (!value.has_value())
        return std::nullopt;
    return boost::uuids::to_string(value.value());
}

inline models::Contract Convert(const pgtypes::ContractV1& value) {
    models::Contract result;
    result.id = boost::uuids::to_string(value.Id);
    result.own_company_id = boost::uuids::to_string(value.OwnCompanyId);
    result.counterparty_id = boost::uuids::to_string(value.CounterpartyId);
    result.number = value.Number;
    result.contract_date = value.ContractDate;
    result.valid_until = value.ValidUntil;
    result.currency = value.Currency;
    result.own_bank_account_id = ConvertId(value.OwnBankAccountId);
    result.counterparty_bank_account_id = ConvertId(value.CounterpartyBankAccountId);
    result.payment_delay_days = value.PaymentDelayDays;
    result.note = value.Note;
    result.archived = value.Archived;
    result.revision = value.Revision;
    return result;
}

inline std::vector<models::Contract> Convert(const pg::ResultSet& rows) {
    std::vector<models::Contract> result;
    result.reserve(rows.Size());
    for (const auto& value : rows.AsSetOf<pgtypes::ContractV1>())
        result.push_back(Convert(value));
    return result;
}

inline pgtypes::ContractV1 Prepare(const models::SaveContractRequest& value) {
    if (value.expected_revision == std::numeric_limits<std::int64_t>::max())
        throw ApiError{400, "invalid_revision", "Некорректная редакция договора"};
    const auto number = userver::utils::text::Trim(value.number);
    if (number.empty())
        throw ApiError{400, "empty_number", "Укажите номер договора"};
    for (const auto& date : {std::optional{value.contract_date}, value.valid_until}) {
        if (date.has_value() &&
            !platform::IsCalendarDateInRange(
                date.value(), std::chrono::year{1900} / 1 / 1, std::chrono::year{9999} / 12 / 31))
            throw ApiError{400, "invalid_date", "Дата должна быть не раньше 01.01.1900"};
    }
    if (value.valid_until.has_value() && value.valid_until.value() < value.contract_date)
        throw ApiError{
            400, "invalid_valid_until", "Срок действия не может быть раньше даты договора"};
    const auto own = boost::uuids::string_generator{}(value.own_company_id);
    const auto counterparty = boost::uuids::string_generator{}(value.counterparty_id);
    if (own == counterparty)
        throw ApiError{400, "same_parties", "Выберите разные стороны договора"};
    auto note = value.note;
    if (note.has_value()) {
        note = userver::utils::text::Trim(note.value());
        if (note.value().empty())
            note.reset();
    }
    return {.Id = boost::uuids::string_generator{}(value.id),
        .OwnCompanyId = own,
        .CounterpartyId = counterparty,
        .Number = number,
        .ContractDate = value.contract_date,
        .ValidUntil = value.valid_until,
        .Currency = value.currency,
        .OwnBankAccountId = ConvertId(value.own_bank_account_id),
        .CounterpartyBankAccountId = ConvertId(value.counterparty_bank_account_id),
        .PaymentDelayDays = value.payment_delay_days,
        .Note = note,
        .Archived = value.archived,
        .Revision = value.expected_revision + 1};
}

inline void ValidateParties(const Deps& deps,
    const userver::server::request::RequestContext& context,
    const models::SaveContractRequest& value) {
    const auto token = platform::auth::GetAccessToken(context);
    try {
        for (const auto& id : {value.own_company_id, value.counterparty_id}) {
            const auto party =
                deps.counterparties.ApiV1CounterpartiesGet({.body = {.id = id}}, token)
                    .counterparty;
            if (party.archived)
                throw ApiError{409, "archived_party", "Выберите действующие стороны договора"};
            if (id == value.own_company_id && !party.own_company)
                throw ApiError{400, "not_own_company", "Выберите собственное юридическое лицо"};
            const auto account = id == value.own_company_id ? value.own_bank_account_id
                                                            : value.counterparty_bank_account_id;
            if (!account.has_value())
                continue;
            const auto accounts = deps.counterparties.ApiV1CounterpartiesBankAccountsList(
                {.body = {.counterparty_id = id}}, token);
            const auto found =
                std::find_if(accounts.items.begin(), accounts.items.end(), [&](const auto& item) {
                    return boost::uuids::string_generator{}(item.id) ==
                        boost::uuids::string_generator{}(account.value());
                });
            if (found == accounts.items.end())
                throw ApiError{400, "invalid_account", "Счёт не принадлежит выбранной стороне"};
            if (found->archived || found->currency != value.currency)
                throw ApiError{
                    400, "invalid_account", "Выберите действующий счёт в валюте договора"};
        }
    } catch (const platform::http::ApiError& error) {
        if (error.GetStatus() == 403)
            throw ApiError{403, "forbidden_party", "Нет доступа к сторонам или счетам договора"};
        if (error.GetStatus() == 404)
            throw ApiError{404, "party_not_found", "Сторона договора не найдена в организации"};
        throw ApiError{502, "counterparty_unavailable", "Не удалось проверить стороны договора"};
    } catch (const platform::http::CancelledError&) {
        throw;
    } catch (const platform::http::TransportError&) {
        throw ApiError{502, "counterparty_unavailable", "Справочник контрагентов недоступен"};
    } catch (const platform::http::ContractError&) {
        throw ApiError{
            502, "counterparty_unavailable", "Некорректный ответ справочника контрагентов"};
    }
}
}  // namespace contracts::helpers
