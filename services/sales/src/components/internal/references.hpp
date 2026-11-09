#pragma once
#include <components/internal/database.hpp>
#include <platform/auth/organization_admin.hpp>
#include <platform/http_client.hpp>
#include <userver/utils/text_light.hpp>

namespace sales::detail {
template <typename Dependencies>
inline auto GetCounterparty(const Dependencies& deps,
    const userver::server::request::RequestContext& context, const std::string& customer) try {
    return deps.counterparties
        .ApiV1CounterpartiesGet({.body = {.id = customer}}, platform::auth::GetAccessToken(context))
        .counterparty;
} catch (const platform::http::ErrorByStatus<403>&) {

    throw ApiError<403>{"reference_forbidden", "Нет доступа к связанным данным"};
} catch (const platform::http::ErrorByStatus<404>&) {
    throw ApiError<404>{"reference_not_found", "Связанный объект не найден в организации"};
} catch (const platform::http::CancelledError&) {
    throw;
} catch (const platform::http::ApiError&) {
    throw ApiError<502>{"reference_unavailable", "Не удалось получить связанные данные"};
} catch (const platform::http::TransportError&) {
    throw ApiError<502>{"reference_unavailable", "Сервис связанных данных недоступен"};
} catch (const platform::http::ContractError&) {
    throw ApiError<502>{"reference_unavailable", "Некорректный ответ сервиса связанных данных"};
}

template <typename Dependencies>
inline void ValidateReferences(const Dependencies& deps,
    const userver::server::request::RequestContext& context, const std::string& customer,
    const std::string& manager) try {
    const auto token = platform::auth::GetAccessToken(context);
    const auto party = GetCounterparty(deps, context, customer);
    if (party.archived)
        throw ApiError<409>{"archived_customer", "Выберите действующего клиента"};
    const auto employees = deps.organizations.ApiOrganizationsEmployeesList(
        {.body = {.limit = 1,
             .organization_id =
                 platform::auth::GetOrganization(platform::auth::GetIdentity(context)),
             .subject = manager}},
        token);
    if (employees.items.empty())
        throw ApiError<400>{"invalid_manager", "Выберите действующего сотрудника организации"};
} catch (const platform::http::ErrorByStatus<403>&) {

    throw ApiError<403>{"reference_forbidden", "Нет доступа к связанным данным"};
} catch (const platform::http::ErrorByStatus<404>&) {
    throw ApiError<404>{"reference_not_found", "Связанный объект не найден в организации"};
} catch (const platform::http::CancelledError&) {
    throw;
} catch (const platform::http::ApiError&) {
    throw ApiError<502>{"reference_unavailable", "Не удалось получить связанные данные"};
} catch (const platform::http::TransportError&) {
    throw ApiError<502>{"reference_unavailable", "Сервис связанных данных недоступен"};
} catch (const platform::http::ContractError&) {
    throw ApiError<502>{"reference_unavailable", "Некорректный ответ сервиса связанных данных"};
}

template <typename Dependencies>
inline void RequireAdministrator(const Dependencies& deps,
    const userver::server::request::RequestContext& context, const std::string& organization) try {
    platform::auth::RequireOrganizationAdministrator(deps.organizations, context, organization);
} catch (const platform::http::ErrorByStatus<403>&) {

    throw ApiError<403>{"reference_forbidden", "Нет доступа к связанным данным"};
} catch (const platform::http::ErrorByStatus<404>&) {
    throw ApiError<404>{"reference_not_found", "Связанный объект не найден в организации"};
} catch (const platform::http::CancelledError&) {
    throw;
} catch (const platform::http::ApiError&) {
    throw ApiError<502>{"reference_unavailable", "Не удалось получить связанные данные"};
} catch (const platform::http::TransportError&) {
    throw ApiError<502>{"reference_unavailable", "Сервис связанных данных недоступен"};
} catch (const platform::http::ContractError&) {
    throw ApiError<502>{"reference_unavailable", "Некорректный ответ сервиса связанных данных"};
}
}  // namespace sales::detail
