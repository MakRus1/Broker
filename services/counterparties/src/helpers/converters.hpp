#pragma once
#include <boost/uuid/string_generator.hpp>
#include <openapi/models.hpp>
#include <postgresql/types.hpp>
#include <userver/storages/postgres/result_set.hpp>
#include <vector>

namespace counterparties::helpers {
inline models::Counterparty Convert(const pgtypes::CounterpartyV3& row) {
    models::Counterparty result;
    result.id = boost::uuids::to_string(row.Id);
    result.name = row.Name;
    result.revision = row.Revision;
    result.archived = row.Archived;
    result.legal_name = row.LegalName;
    result.country_code = row.CountryCode;
    result.tax_id = row.TaxId;
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

inline pgtypes::CounterpartyV3 Convert(const models::Counterparty& value) {
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
        .Contacts = userver::formats::json::ValueBuilder(value.contacts).ExtractValue()};
}

inline std::vector<models::Counterparty> Convert(
    const userver::storages::postgres::ResultSet& rows) {
    std::vector<models::Counterparty> result;
    result.reserve(rows.Size());
    for (const auto& row : rows.AsSetOf<pgtypes::CounterpartyV3>())
        result.push_back(Convert(row));
    return result;
}
}  // namespace counterparties::helpers
