#pragma once
#include <algorithm>
#include <boost/uuid/string_generator.hpp>
#include <models/errors.hpp>
#include <string>

namespace finance::helpers {

inline bool SameId(const std::string& left, const std::string& right) {
    return boost::uuids::string_generator{}(left) == boost::uuids::string_generator{}(right);
}

template <typename Request, typename Party>
void ValidateDocumentParty(const Request& body, const Party& party, bool own_company) {
    if (party.archived && !body.archived)
        throw ApiError<409>{"archived_party", "Выберите действующие стороны документа"};
    if (own_company && !party.own_company)
        throw ApiError<400>{"not_own_company", "Выберите собственное юрлицо"};
}

template <typename Request, typename Accounts>
void ValidateDocumentAccount(const Request& body, const Accounts& accounts, const std::string& id) {
    const auto found = std::find_if(
        accounts.begin(), accounts.end(), [&](const auto& item) { return SameId(item.id, id); });
    if (found == accounts.end() || found->currency != body.currency ||
        (found->archived && !body.archived))
        throw ApiError<400>{"invalid_account", "Выберите счёт стороны в валюте документа"};
}

template <typename Request, typename Contract>
void ValidateDocumentContract(const Request& body, const Contract& contract) {
    if (!SameId(contract.own_company_id, body.own_company_id) ||
        !SameId(contract.counterparty_id, body.counterparty_id) ||
        contract.currency != body.currency)
        throw ApiError<400>{"wrong_contract", "Стороны и валюта должны соответствовать договору"};
    if (contract.archived && !body.archived)
        throw ApiError<409>{"archived_contract", "Выберите действующий договор"};
}
}  // namespace finance::helpers
