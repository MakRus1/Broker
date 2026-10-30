#pragma once
#include <boost/uuid/string_generator.hpp>
#include <deps.hpp>
#include <models/errors.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <platform/http_client.hpp>

namespace finance::helpers {
inline bool SameId(const std::string& left, const std::string& right) {
    return boost::uuids::string_generator{}(left) == boost::uuids::string_generator{}(right);
}

inline void ValidateEntryScope(const Deps& deps,
    const userver::server::request::RequestContext& context, const models::EntryScope& scope) {
    const auto token = platform::auth::GetAccessToken(context);
    try {
        if (scope.transportation_id.has_value()) {
            const auto transportation =
                deps.transportations
                    .ApiV1TransportationsGet(
                        {.body = {.transportation_id = scope.transportation_id.value()}}, token)
                    .transportation;
            if (!SameId(transportation.shipment_id, scope.shipment_id))
                throw ApiError<400>{"wrong_shipment", "Перевозка относится к другой поставке"};
        } else {
            deps.shipments.ApiV1ShipmentsGet({.body = {.shipment_id = scope.shipment_id}}, token);
        }
    } catch (const platform::http::ErrorByStatus<403>&) {
        throw ApiError<403>{"forbidden_reference", "Нет доступа к связанным данным начисления"};
    } catch (const platform::http::ErrorByStatus<404>&) {
        throw ApiError<404>{"reference_not_found", "Связанный объект не найден в организации"};
    } catch (const platform::http::ApiError&) {
        throw ApiError<502>{
            "reference_unavailable", "Не удалось проверить связанные данные начисления"};
    } catch (const platform::http::CancelledError&) {
        throw;
    } catch (const platform::http::TransportError&) {
        throw ApiError<502>{"reference_unavailable", "Сервис связанных данных недоступен"};
    } catch (const platform::http::ContractError&) {
        throw ApiError<502>{"reference_unavailable", "Некорректный ответ сервиса связанных данных"};
    }
}

struct EntryParties {
    std::string CounterpartyName;
    std::optional<std::string> ContractNumber;
};

inline EntryParties ValidateEntryParties(const Deps& deps,
    const userver::server::request::RequestContext& context,
    const models::SaveFinancialEntryRequest& value) {
    const auto token = platform::auth::GetAccessToken(context);
    try {
        const auto party =
            deps.counterparties
                .ApiV1CounterpartiesGet({.body = {.id = value.counterparty_id}}, token)
                .counterparty;
        if (party.archived && !value.archived)
            throw ApiError<409>{"archived_party", "Выберите действующего контрагента"};
        EntryParties result{.CounterpartyName = party.name, .ContractNumber = std::nullopt};
        if (value.contract_id.has_value()) {
            const auto contract =
                deps.contracts.ApiV1ContractsGet({.body = {.id = value.contract_id.value()}}, token)
                    .contract;
            if (!SameId(contract.counterparty_id, value.counterparty_id))
                throw ApiError<400>{"wrong_contract", "Договор относится к другому контрагенту"};
            if (contract.currency != value.currency)
                throw ApiError<400>{
                    "contract_currency", "Валюта начисления должна совпадать с валютой договора"};
            if (contract.archived && !value.archived)
                throw ApiError<409>{"archived_contract", "Выберите действующий договор"};
            result.ContractNumber = contract.number;
        }
        return result;
    } catch (const platform::http::ErrorByStatus<403>&) {
        throw ApiError<403>{"forbidden_reference", "Нет доступа к связанным данным начисления"};
    } catch (const platform::http::ErrorByStatus<404>&) {
        throw ApiError<404>{"reference_not_found", "Связанный объект не найден в организации"};
    } catch (const platform::http::ApiError&) {
        throw ApiError<502>{
            "reference_unavailable", "Не удалось проверить связанные данные начисления"};
    } catch (const platform::http::CancelledError&) {
        throw;
    } catch (const platform::http::TransportError&) {
        throw ApiError<502>{"reference_unavailable", "Сервис связанных данных недоступен"};
    } catch (const platform::http::ContractError&) {
        throw ApiError<502>{"reference_unavailable", "Некорректный ответ сервиса связанных данных"};
    }
}
}  // namespace finance::helpers
