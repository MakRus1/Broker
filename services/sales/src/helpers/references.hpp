#pragma once
#include <helpers/storage.hpp>
#include <platform/auth/organization_admin.hpp>
#include <platform/http_client.hpp>
#include <userver/utils/text_light.hpp>

namespace sales::helpers {
// Ошибки внешнего контракта переводим на границе клиента, не маскируя ошибки БД.
template <typename Call> auto Reference(Call&& call) {
    try {
        return call();
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
}

inline void ValidateReferences(const Deps& deps,
    const userver::server::request::RequestContext& context, const std::string& customer,
    const std::string& manager) {
    const auto token = platform::auth::GetAccessToken(context);
    const auto party = Reference([&] {
        return deps.counterparties.ApiV1CounterpartiesGet({.body = {.id = customer}}, token);
    });
    if (party.counterparty.archived)
        throw ApiError<409>{"archived_customer", "Выберите действующего клиента"};
    const auto employees = Reference([&] {
        return deps.organizations.ApiOrganizationsEmployeesList(
            {.body = {.limit = 1, .organization_id = Organization(context), .subject = manager}},
            token);
    });
    if (employees.items.empty())
        throw ApiError<400>{"invalid_manager", "Выберите действующего сотрудника организации"};
}

inline void RequireAdministrator(const Deps& deps,
    const userver::server::request::RequestContext& context, const std::string& organization) {
    Reference([&] {
        platform::auth::RequireOrganizationAdministrator(deps.organizations, context, organization);
    });
}
}  // namespace sales::helpers
