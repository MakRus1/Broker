#include <algorithm>
#include <cctype>
#include <helpers/counterparty_validation.hpp>
#include <models/errors.hpp>
#include <platform/contact_validation.hpp>
#include <platform/text.hpp>

namespace counterparties::helpers {
models::Counterparty NormalizeAndValidateCounterparty(models::Counterparty value) {
    value.name = userver::utils::text::Trim(value.name);
    value.legal_name = platform::TrimToOptional(value.legal_name);
    value.country_code = platform::TrimToOptional(value.country_code);
    if (value.country_code.has_value()) {
        auto& country = value.country_code.value();
        std::transform(country.begin(), country.end(), country.begin(),
            [](unsigned char c) { return std::toupper(c); });
    }
    value.tax_id = platform::TrimToOptional(value.tax_id);
    value.kpp.emplace(platform::TrimToOptional(value.kpp.value_or(std::nullopt)));
    value.logist.emplace(value.logist.value_or(std::nullopt));
    value.registration_id = platform::TrimToOptional(value.registration_id);
    value.legal_address = platform::TrimToOptional(value.legal_address);
    std::sort(value.roles.begin(), value.roles.end());
    if (value.name.empty())
        throw ApiError<400>{"empty_name", "Укажите название контрагента"};
    if (value.tax_id.has_value() && !value.country_code.has_value())
        throw ApiError<400>{"country_required", "Укажите страну для налогового номера"};
    for (auto& contact : value.contacts) {
        contact.name = userver::utils::text::Trim(contact.name);
        contact.position.emplace(platform::TrimToOptional(contact.position.value_or(std::nullopt)));
        const auto email = platform::TrimToOptional(contact.email.value_or(std::nullopt));
        contact.email.emplace(email);
        const auto phone = platform::TrimToOptional(contact.phone.value_or(std::nullopt));
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
