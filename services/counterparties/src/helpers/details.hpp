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

template <typename Request> models::Counterparty Prepare(const Request& input) {
    models::Counterparty value;
    value.id = boost::uuids::to_string(boost::uuids::string_generator{}(input.id));
    value.revision = 1;
    value.archived = false;
    value.name = userver::utils::text::Trim(input.name);
    value.legal_name = userver::utils::text::Trim(input.legal_name);
    value.country_code = input.country_code;
    std::transform(value.country_code.begin(), value.country_code.end(), value.country_code.begin(),
        [](unsigned char c) { return std::toupper(c); });
    for (unsigned char c : input.tax_id)
        if (!std::isspace(c) && c != '-')
            value.tax_id += static_cast<char>(std::toupper(c));
    value.registration_id = userver::utils::text::Trim(input.registration_id);
    value.legal_address = userver::utils::text::Trim(input.legal_address);
    value.roles = input.roles;
    std::sort(value.roles.begin(), value.roles.end());
    value.contacts = input.contacts.value_or(std::vector<models::CounterpartyContact>{});
    if (value.name.empty())
        throw ApiError{400, "empty_name", "Укажите название контрагента"};
    if (!value.tax_id.empty() && value.country_code.empty())
        throw ApiError{400, "country_required", "Укажите страну для налогового номера"};
    for (auto& contact : value.contacts) {
        contact.name = userver::utils::text::Trim(contact.name);
        contact.position = userver::utils::text::Trim(contact.position);
        contact.email = userver::utils::text::Trim(contact.email);
        contact.phone = userver::utils::text::Trim(contact.phone);
        if (contact.name.empty())
            throw ApiError{400, "empty_contact", "Укажите имя контактного лица"};
        if (!contact.email.empty() && contact.email.find('@') == std::string::npos)
            throw ApiError{400, "invalid_email", "Проверьте email контактного лица"};
    }
    return value;
}

inline void CheckDuplicate(pg::Transaction& transaction, const std::string& organization,
    const models::Counterparty& value) {
    if (value.tax_id.empty())
        return;
    transaction.Execute(
        sql::kLockRequisites, organization + ":" + value.country_code + ":" + value.tax_id);
    const auto duplicate = transaction.Execute(
        sql::kFindDuplicate, organization, value.country_code, value.tax_id, value.id);
    if (!duplicate.IsEmpty())
        throw ApiError{409, "duplicate_requisites",
            "Такие реквизиты уже есть у контрагента: " + duplicate.AsSingleRow<std::string>()};
}
}  // namespace counterparties::helpers
