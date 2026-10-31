#pragma once
#include <helpers/service_charge_references.hpp>

namespace finance::helpers {
template <typename Request, typename Row>
void ValidateDocumentParties(const Deps& deps,
    const userver::server::request::RequestContext& context, const Request& body, Row& result) {
    const auto token = platform::auth::GetAccessToken(context);
    try {
        for (const auto& id : {body.own_company_id, body.counterparty_id}) {
            const auto party =
                deps.counterparties.ApiV1CounterpartiesGet({.body = {.id = id}}, token)
                    .counterparty;
            if (party.archived && !body.archived)
                throw ApiError<409>{"archived_party", "Выберите действующие стороны документа"};
            if (id == body.own_company_id) {
                if (!party.own_company)
                    throw ApiError<400>{"not_own_company", "Выберите собственное юрлицо"};
                result.OwnCompanyName = party.name;
            } else
                result.CounterpartyName = party.name;
            const auto account = id == body.own_company_id ? body.own_bank_account_id
                                                           : body.counterparty_bank_account_id;
            if (!account.has_value())
                continue;
            const auto accounts = deps.counterparties.ApiV1CounterpartiesBankAccountsList(
                {.body = {.counterparty_id = id}}, token);
            const auto found = std::find_if(accounts.items.begin(), accounts.items.end(),
                [&](const auto& item) { return SameId(item.id, account.value()); });
            if (found == accounts.items.end() || found->currency != body.currency ||
                (found->archived && !body.archived))
                throw ApiError<400>{"invalid_account", "Выберите счёт стороны в валюте документа"};
        }
        if constexpr (requires { body.contract_id; }) {
            if (body.contract_id.has_value()) {
                const auto contract =
                    deps.contracts
                        .ApiV1ContractsGet({.body = {.id = body.contract_id.value()}}, token)
                        .contract;
                if (!SameId(contract.own_company_id, body.own_company_id) ||
                    !SameId(contract.counterparty_id, body.counterparty_id) ||
                    contract.currency != body.currency)
                    throw ApiError<400>{
                        "wrong_contract", "Стороны и валюта должны соответствовать договору"};
                if (contract.archived && !body.archived)
                    throw ApiError<409>{"archived_contract", "Выберите действующий договор"};
                result.ContractNumber = contract.number;
            }
        }
    } catch (const platform::http::ErrorByStatus<403>&) {
        throw ApiError<403>{"forbidden_reference", "Нет доступа к сторонам или договору документа"};
    } catch (const platform::http::ErrorByStatus<404>&) {
        throw ApiError<404>{"reference_not_found", "Сторона или договор не найдены в организации"};
    } catch (const platform::http::ApiError&) {
        throw ApiError<502>{"reference_unavailable", "Не удалось проверить стороны документа"};
    } catch (const platform::http::CancelledError&) {
        throw;
    } catch (const platform::http::TransportError&) {
        throw ApiError<502>{"reference_unavailable", "Сервис связанных данных недоступен"};
    } catch (const platform::http::ContractError&) {
        throw ApiError<502>{"reference_unavailable", "Некорректный ответ сервиса связанных данных"};
    }
}
}  // namespace finance::helpers
