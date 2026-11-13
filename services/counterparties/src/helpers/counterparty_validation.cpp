#include <algorithm>
#include <cctype>
#include <helpers/counterparty_validation.hpp>
#include <helpers/normalization.hpp>
#include <models/errors.hpp>
#include <platform/contact_validation.hpp>

namespace counterparties::helpers {
models::Counterparty NormalizeAndValidateCounterparty(models::Counterparty value) {
    value.name = userver::utils::text::Trim(value.name);
    value.legal_name = Normalize(value.legal_name);
    value.country_code = Normalize(value.country_code);
    if (value.country_code.has_value()) {
        auto& country = value.country_code.value();
        std::transform(country.begin(), country.end(), country.begin(),
            [](unsigned char c) { return std::toupper(c); });
    }
    value.tax_id = Normalize(value.tax_id);
    value.kpp.emplace(Normalize(value.kpp.value_or(std::nullopt)));
    value.logist.emplace(value.logist.value_or(std::nullopt));
    value.registration_id = Normalize(value.registration_id);
    value.legal_address = Normalize(value.legal_address);
    std::sort(value.roles.begin(), value.roles.end());
    if (value.name.empty())
        throw ApiError<400>{"empty_name", "Укажите название контрагента"};
    if (value.tax_id.has_value() && !value.country_code.has_value())
        throw ApiError<400>{"country_required", "Укажите страну для налогового номера"};
    for (auto& contact : value.contacts) {
        contact.name = userver::utils::text::Trim(contact.name);
        contact.position.emplace(Normalize(contact.position.value_or(std::nullopt)));
        const auto email = Normalize(contact.email.value_or(std::nullopt));
        contact.email.emplace(email);
        const auto phone = Normalize(contact.phone.value_or(std::nullopt));
        contact.phone.emplace(phone);
        if (phone.has_value() && !platform::IsPhone(phone.value()))
            throw ApiError<400>{"invalid_phone", "Телефон должен содержать от 7 до 15 цифр"};
        if (contact.name.empty())
            throw ApiError<400>{"empty_contact", "Укажите имя контактного лица"};
        if (email.has_value() && !platform::IsEmail(email.value()))
            throw ApiError<400>{"invalid_email", "Проверьте email контактного лица"};
    }
    return value;
}

}  // namespace counterparties::helpers
