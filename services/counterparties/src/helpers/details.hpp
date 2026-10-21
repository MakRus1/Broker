#pragma once
#include <algorithm>
#include <boost/uuid/string_generator.hpp>
#include <cctype>
#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <userver/utils/text_light.hpp>

namespace counterparties::helpers {
template <typename Value> auto Document(const Value& value) {
    return userver::formats::json::ValueBuilder(value).ExtractValue();
}

inline std::optional<std::string> Normalize(std::optional<std::string> value) {
    if (!value.has_value())
        return std::nullopt;
    value = userver::utils::text::Trim(value.value());
    return value.value().empty() ? std::nullopt : value;
}

inline models::Counterparty Normalize(models::Counterparty value) {
    value.name = userver::utils::text::Trim(value.name);
    value.legal_name = Normalize(value.legal_name);
    value.country_code = Normalize(value.country_code);
    if (value.country_code.has_value()) {
        auto& country = value.country_code.value();
        std::transform(country.begin(), country.end(), country.begin(),
            [](unsigned char c) { return std::toupper(c); });
    }
    value.tax_id = Normalize(value.tax_id);
    value.registration_id = Normalize(value.registration_id);
    value.legal_address = Normalize(value.legal_address);
    std::sort(value.roles.begin(), value.roles.end());
    if (value.name.empty())
        throw ApiError{400, "empty_name", "Укажите название контрагента"};
    if (value.tax_id.has_value() && !value.country_code.has_value())
        throw ApiError{400, "country_required", "Укажите страну для налогового номера"};
    for (auto& contact : value.contacts) {
        contact.name = userver::utils::text::Trim(contact.name);
        contact.position.emplace(Normalize(contact.position.value_or(std::nullopt)));
        const auto email = Normalize(contact.email.value_or(std::nullopt));
        contact.email.emplace(email);
        contact.phone.emplace(Normalize(contact.phone.value_or(std::nullopt)));
        if (contact.name.empty())
            throw ApiError{400, "empty_contact", "Укажите имя контактного лица"};
        if (email.has_value() && email.value().find('@') == std::string::npos)
            throw ApiError{400, "invalid_email", "Проверьте email контактного лица"};
    }
    return value;
}

inline models::Counterparty Prepare(const models::CreateCounterpartyRequest& input) {
    models::Counterparty value;
    value.id = boost::uuids::to_string(boost::uuids::string_generator{}(input.id));
    value.revision = 1;
    value.archived = false;
    value.name = input.name;
    value.legal_name = input.legal_name.value_or(std::nullopt);
    value.country_code = input.country_code.value_or(std::nullopt);
    value.tax_id = input.tax_id.value_or(std::nullopt);
    value.registration_id = input.registration_id.value_or(std::nullopt);
    value.legal_address = input.legal_address.value_or(std::nullopt);
    value.roles = input.roles;
    value.contacts = input.contacts.value_or(std::vector<models::CounterpartyContact>{});
    return Normalize(std::move(value));
}

inline models::Counterparty Prepare(
    const models::UpdateCounterpartyRequest& input, models::Counterparty value) {
    value.name = input.name;
    if (input.legal_name.has_value())
        value.legal_name = input.legal_name.value();
    if (input.country_code.has_value())
        value.country_code = input.country_code.value();
    if (input.tax_id.has_value())
        value.tax_id = input.tax_id.value();
    if (input.registration_id.has_value())
        value.registration_id = input.registration_id.value();
    if (input.legal_address.has_value())
        value.legal_address = input.legal_address.value();
    if (input.roles.has_value())
        value.roles = input.roles.value();
    if (input.contacts.has_value())
        value.contacts = input.contacts.value();
    return Normalize(std::move(value));
}

inline void CheckDuplicate(pg::Transaction& transaction, const std::string& organization,
    const models::Counterparty& value) {
    if (!value.tax_id.has_value())
        return;
    transaction.Execute(sql::kLockRequisites,
        organization + ":" + value.country_code.value() + ":" + value.tax_id.value());
    const auto duplicate = transaction.Execute(
        sql::kFindDuplicate, organization, value.country_code, value.tax_id, value.id);
    if (!duplicate.IsEmpty())
        throw ApiError{409, "duplicate_requisites",
            "Такие реквизиты уже есть у контрагента: " + duplicate.AsSingleRow<std::string>()};
}
}  // namespace counterparties::helpers
