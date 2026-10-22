#pragma once
#include <clients/counterparties/client.hpp>
#include <openapi/models.hpp>
#include <stdexcept>

namespace web_bff {
inline models::counterparties::RegistryParty Convert(
    const clients::counterparties::models::RegistryParty& value) {
    return {.kpp = value.kpp,
        .legal_address = value.legal_address,
        .legal_name = value.legal_name,
        .name = value.name,
        .registration_id = value.registration_id,
        .status = value.status,
        .tax_id = value.tax_id};
}

inline models::counterparties::CounterpartyRole Convert(
    clients::counterparties::models::CounterpartyRole value) {
    switch (value) {
    case clients::counterparties::models::CounterpartyRole::Customer:
        return models::counterparties::CounterpartyRole::Customer;
    case clients::counterparties::models::CounterpartyRole::Carrier:
        return models::counterparties::CounterpartyRole::Carrier;
    case clients::counterparties::models::CounterpartyRole::Agent:
        return models::counterparties::CounterpartyRole::Agent;
    case clients::counterparties::models::CounterpartyRole::Supplier:
        return models::counterparties::CounterpartyRole::Supplier;
    case clients::counterparties::models::CounterpartyRole::Partner:
        return models::counterparties::CounterpartyRole::Partner;
    }
    throw std::invalid_argument("Unknown counterparty role");
}

inline models::counterparties::CounterpartyContact Convert(
    const clients::counterparties::models::CounterpartyContact& value) {
    return {
        .email = value.email, .name = value.name, .phone = value.phone, .position = value.position};
}

inline clients::counterparties::models::CounterpartyRole Convert(
    models::counterparties::CounterpartyRole value) {
    switch (value) {
    case models::counterparties::CounterpartyRole::Customer:
        return clients::counterparties::models::CounterpartyRole::Customer;
    case models::counterparties::CounterpartyRole::Carrier:
        return clients::counterparties::models::CounterpartyRole::Carrier;
    case models::counterparties::CounterpartyRole::Agent:
        return clients::counterparties::models::CounterpartyRole::Agent;
    case models::counterparties::CounterpartyRole::Supplier:
        return clients::counterparties::models::CounterpartyRole::Supplier;
    case models::counterparties::CounterpartyRole::Partner:
        return clients::counterparties::models::CounterpartyRole::Partner;
    }
    throw std::invalid_argument("Unknown counterparty role");
}

inline clients::counterparties::models::CounterpartyContact Convert(
    const models::counterparties::CounterpartyContact& value) {
    return {
        .email = value.email, .name = value.name, .phone = value.phone, .position = value.position};
}

inline models::counterparties::Counterparty Convert(
    const clients::counterparties::models::Counterparty& value) {
    models::counterparties::Counterparty result;
    result.id = value.id;
    result.name = value.name;
    result.revision = value.revision;
    result.archived = value.archived;
    result.legal_name = value.legal_name;
    result.country_code = value.country_code;
    result.tax_id = value.tax_id;
    result.kpp = value.kpp;
    result.registration_id = value.registration_id;
    result.legal_address = value.legal_address;
    for (const auto role : value.roles)
        result.roles.push_back(Convert(role));
    for (const auto& contact : value.contacts)
        result.contacts.push_back(Convert(contact));
    return result;
}

inline clients::counterparties::ApiV1CounterpartiesCreate::RequestBody Convert(
    const models::counterparties::CreateCounterpartyRequest& value) {
    clients::counterparties::ApiV1CounterpartiesCreate::RequestBody result;
    result.id = value.id;
    result.name = value.name;
    result.legal_name = value.legal_name;
    result.country_code = value.country_code;
    result.tax_id = value.tax_id;
    result.kpp = value.kpp;
    result.registration_id = value.registration_id;
    result.legal_address = value.legal_address;
    for (const auto role : value.roles)
        result.roles.push_back(Convert(role));
    if (value.contacts.has_value()) {
        result.contacts.emplace();
        for (const auto& contact : value.contacts.value())
            result.contacts.value().push_back(Convert(contact));
    }
    return result;
}

inline clients::counterparties::ApiV1CounterpartiesUpdate::RequestBody Convert(
    const models::counterparties::UpdateCounterpartyRequest& value) {
    clients::counterparties::ApiV1CounterpartiesUpdate::RequestBody result;
    result.id = value.id;
    result.name = value.name;
    result.legal_name = value.legal_name;
    result.country_code = value.country_code;
    result.tax_id = value.tax_id;
    result.kpp = value.kpp;
    result.registration_id = value.registration_id;
    result.legal_address = value.legal_address;
    result.expected_revision = value.expected_revision;
    if (value.roles.has_value()) {
        result.roles.emplace();
        for (const auto role : value.roles.value())
            result.roles.value().push_back(Convert(role));
    }
    if (value.contacts.has_value()) {
        result.contacts.emplace();
        for (const auto& contact : value.contacts.value())
            result.contacts.value().push_back(Convert(contact));
    }
    return result;
}

inline clients::counterparties::ApiV1CounterpartiesGet::RequestBody Convert(
    const models::counterparties::GetCounterpartyRequest& value) {
    return {.id = value.id};
}

inline clients::counterparties::ApiV1CounterpartiesSetArchived::RequestBody Convert(
    const models::counterparties::SetArchivedCounterpartyRequest& value) {
    return {
        .archived = value.archived, .expected_revision = value.expected_revision, .id = value.id};
}

inline clients::counterparties::ApiV1CounterpartiesList::RequestBody Convert(
    const models::counterparties::ListCounterpartiesRequest& value) {
    clients::counterparties::ApiV1CounterpartiesList::RequestBody result;
    result.after = value.after;
    result.limit = value.limit;
    result.query = value.query;
    if (value.role.has_value())
        result.role = Convert(value.role.value());
    // Enum фильтра принадлежит собственному контракту каждого участника.
    switch (value.status) {
    case models::counterparties::ListCounterpartiesRequest_status::Active:
        result.status = clients::counterparties::models::ListCounterpartiesRequest_status::Active;
        break;
    case models::counterparties::ListCounterpartiesRequest_status::Archived:
        result.status = clients::counterparties::models::ListCounterpartiesRequest_status::Archived;
        break;
    case models::counterparties::ListCounterpartiesRequest_status::All:
        result.status = clients::counterparties::models::ListCounterpartiesRequest_status::All;
        break;
    }
    return result;
}

inline clients::counterparties::ApiV1CounterpartiesHistory::RequestBody Convert(
    const models::counterparties::ListCounterpartyHistoryRequest& value) {
    return {.before = value.before, .id = value.id, .limit = value.limit};
}

inline models::counterparties::CounterpartySnapshot Convert(
    const clients::counterparties::models::CounterpartySnapshot& value) {
    models::counterparties::CounterpartySnapshot result;
    result.name = value.name;
    result.legal_name = value.legal_name;
    result.country_code = value.country_code;
    result.tax_id = value.tax_id;
    result.kpp = value.kpp;
    result.registration_id = value.registration_id;
    result.legal_address = value.legal_address;
    result.archived = value.archived;
    if (value.roles.has_value()) {
        result.roles.emplace();
        for (const auto role : value.roles.value())
            result.roles.value().push_back(Convert(role));
    }
    if (value.contacts.has_value()) {
        result.contacts.emplace();
        for (const auto& contact : value.contacts.value())
            result.contacts.value().push_back(Convert(contact));
    }
    return result;
}

inline models::counterparties::CounterpartyEvent Convert(
    const clients::counterparties::models::CounterpartyEvent& value) {
    models::counterparties::CounterpartyEvent result;
    result.id = value.id;
    result.action = value.action;
    result.actor = value.actor;
    result.actor_name = value.actor_name;
    result.occurred_at = value.occurred_at;
    result.revision = value.revision;
    result.after = Convert(value.after);
    if (value.before.has_value())
        result.before = Convert(value.before.value());
    return result;
}
}  // namespace web_bff
