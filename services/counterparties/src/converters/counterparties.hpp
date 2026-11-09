#pragma once
#include <boost/uuid/string_generator.hpp>
#include <openapi/models.hpp>
#include <postgresql/types.hpp>
#include <vector>

namespace counterparties::converters {
inline models::Counterparty Convert(const pgtypes::CounterpartyV6& row) {
    models::Counterparty result;
    result.id = boost::uuids::to_string(row.Id);
    result.name = row.Name;
    result.revision = row.Revision;
    result.archived = row.Archived;
    result.own_company = row.OwnCompany;
    result.legal_name = row.LegalName;
    result.country_code = row.CountryCode;
    result.tax_id = row.TaxId;
    result.kpp = row.Kpp;
    result.logist = row.Logist;
    result.registration_id = row.RegistrationId;
    result.legal_address = row.LegalAddress;
    for (const auto& role : row.Roles)
        result.roles.push_back(models::ParseEnumScalar(role, models::CounterpartyRole{}));
    for (const auto& contact : row.Contacts)
        result.contacts.push_back(
            models::ParseModelJson(contact, std::type_identity<models::CounterpartyContact>{}));
    return result;
}

inline std::vector<std::string> Convert(const std::vector<models::CounterpartyRole>& roles) {
    std::vector<std::string> result;
    for (const auto role : roles)
        result.push_back(models::EnumValue(role));
    return result;
}

inline pgtypes::CounterpartyV6 Convert(const models::Counterparty& value) {
    return {.Id = boost::uuids::string_generator{}(value.id),
        .Name = value.name,
        .Revision = value.revision,
        .Archived = value.archived,
        .LegalName = value.legal_name,
        .CountryCode = value.country_code,
        .TaxId = value.tax_id,
        .RegistrationId = value.registration_id,
        .LegalAddress = value.legal_address,
        .Roles = Convert(value.roles),
        .Contacts = userver::formats::json::ValueBuilder(value.contacts).ExtractValue(),
        .Kpp = value.kpp.value_or(std::nullopt),
        .Logist = value.logist.value_or(std::nullopt),
        .OwnCompany = value.own_company};
}

inline models::Counterparty Convert(const models::CreateCounterpartyRequest& input) {
    models::Counterparty value;
    value.id = boost::uuids::to_string(boost::uuids::string_generator{}(input.id));
    value.revision = 1;
    value.archived = false;
    value.own_company = input.own_company.value_or(false);
    value.name = input.name;
    value.legal_name = input.legal_name.value_or(std::nullopt);
    value.country_code = input.country_code.value_or(std::nullopt);
    value.tax_id = input.tax_id.value_or(std::nullopt);
    value.kpp = input.kpp;
    value.logist = input.logist;
    value.registration_id = input.registration_id.value_or(std::nullopt);
    value.legal_address = input.legal_address.value_or(std::nullopt);
    value.roles = input.roles;
    value.contacts = input.contacts.value_or(std::vector<models::CounterpartyContact>{});
    return value;
}

inline models::Counterparty Convert(
    const models::UpdateCounterpartyRequest& input, models::Counterparty value) {
    value.name = input.name;
    if (input.own_company.has_value())
        value.own_company = input.own_company.value();
    if (input.legal_name.has_value())
        value.legal_name = input.legal_name.value();
    if (input.country_code.has_value())
        value.country_code = input.country_code.value();
    if (input.tax_id.has_value())
        value.tax_id = input.tax_id.value();
    if (input.logist.has_value())
        value.logist = input.logist;
    if (input.kpp.has_value())
        value.kpp = input.kpp;
    if (input.registration_id.has_value())
        value.registration_id = input.registration_id.value();
    if (input.legal_address.has_value())
        value.legal_address = input.legal_address.value();
    if (input.roles.has_value())
        value.roles = input.roles.value();
    if (input.contacts.has_value())
        value.contacts = input.contacts.value();
    return value;
}

}  // namespace counterparties::converters
