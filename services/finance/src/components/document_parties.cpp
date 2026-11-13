#include <algorithm>
#include <components/document_parties.hpp>
#include <helpers/document_parties.hpp>
#include <models/errors.hpp>
#include <platform/http_client.hpp>
#include <userver/components/component_context.hpp>

namespace finance {
DocumentPartiesComponent::DocumentPartiesComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      counterparties_(context.FindComponent<clients::counterparties::Component>().GetClient()),
      contracts_(context.FindComponent<clients::contracts::Component>().GetClient()) {}

userver::yaml_config::Schema DocumentPartiesComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::DocumentParties DocumentPartiesComponent::ResolveDocumentParties(
    std::string_view access_token, const models::DocumentPartiesInput& input) const {
    models::DocumentParties result;
    try {
        for (const bool own_company : {true, false}) {
            const auto& id = own_company ? input.OwnCompanyId : input.CounterpartyId;
            const auto party =
                counterparties_.ApiV1CounterpartiesGet({.body = {.id = id}}, access_token)
                    .counterparty;
            if (party.archived && !input.Archived)
                throw ApiError<409>{"archived_party", "Выберите действующие стороны документа"};
            if (own_company && !party.own_company)
                throw ApiError<400>{"not_own_company", "Выберите собственное юрлицо"};
            (own_company ? result.OwnCompanyName : result.CounterpartyName) = party.name;

            const auto& account_id =
                own_company ? input.OwnBankAccountId : input.CounterpartyBankAccountId;
            if (account_id) {
                const auto accounts = counterparties_.ApiV1CounterpartiesBankAccountsList(
                    {.body = {.counterparty_id = id}}, access_token);
                const auto account = std::find_if(accounts.items.begin(), accounts.items.end(),
                    [&](const auto& item) { return helpers::SameId(item.id, *account_id); });
                if (account == accounts.items.end() || account->currency != input.Currency ||
                    (account->archived && !input.Archived))
                    throw ApiError<400>{
                        "invalid_account", "Выберите счёт стороны в валюте документа"};
            }
        }

        if (input.ContractId) {
            const auto contract =
                contracts_.ApiV1ContractsGet({.body = {.id = *input.ContractId}}, access_token)
                    .contract;
            if (!helpers::SameId(contract.own_company_id, input.OwnCompanyId) ||
                !helpers::SameId(contract.counterparty_id, input.CounterpartyId) ||
                contract.currency != input.Currency)
                throw ApiError<400>{
                    "wrong_contract", "Стороны и валюта должны соответствовать договору"};
            if (contract.archived && !input.Archived)
                throw ApiError<409>{"archived_contract", "Выберите действующий договор"};
            result.ContractNumber = contract.number;
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
    return result;
}
}  // namespace finance
