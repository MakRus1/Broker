#pragma once
#include <deps.hpp>
#include <helpers/database.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <platform/http_client.hpp>

namespace shipments::helpers {
inline auto FindEmployees(const Deps& deps, const userver::server::request::RequestContext& context,
    const std::optional<std::string>& query = std::nullopt,
    const std::optional<std::string>& subject = std::nullopt, int limit = 20) try {
    namespace api = clients::organizations::ApiOrganizationsEmployeesList;
    const auto& identity = platform::auth::GetIdentity(context);
    const auto response = deps.organizations.ApiOrganizationsEmployeesList(
        api::Request{.body = {.limit = limit,
                         .organization_id = platform::auth::GetOrganization(identity),
                         .query = query,
                         .subject = subject}},
        platform::auth::GetAccessToken(context));
    return response.items;
} catch (const platform::http::ApiError&) {

    throw ApiError{503, "employee_directory_unavailable",
        "Не удалось проверить сотрудника. Повторите сохранение позже"};
} catch (const platform::http::CancelledError&) {
    throw;
} catch (const platform::http::TransportError&) {
    throw ApiError{503, "employee_directory_unavailable",
        "Не удалось проверить сотрудника. Повторите сохранение позже"};
} catch (const platform::http::ContractError&) {
    throw ApiError{
        503, "employee_directory_unavailable", "Некорректный ответ справочника сотрудников"};
}

inline void ValidateLogist(const Deps& deps,
    const userver::server::request::RequestContext& context,
    const std::optional<std::string>& subject) {
    if (subject.has_value() && FindEmployees(deps, context, std::nullopt, subject, 1).empty())
        throw ApiError{400, "invalid_logist", "Выберите действующего сотрудника организации"};
}

inline std::optional<std::string> CustomerLogist(const Deps& deps,
    const userver::server::request::RequestContext& context, const std::string& customer) try {
    namespace api = clients::counterparties::ApiV1CounterpartiesGet;
    const auto result = deps.counterparties.ApiV1CounterpartiesGet(
        api::Request{.body = {.id = customer}}, platform::auth::GetAccessToken(context));
    return result.counterparty.logist.value_or(std::nullopt);
} catch (const platform::http::ErrorByStatus<404>&) {

    throw ApiError{400, "invalid_customer", "Клиент не найден в организации"};
} catch (const platform::http::ApiError&) {
    throw ApiError{503, "customer_directory_unavailable", "Не удалось получить карточку клиента"};
} catch (const platform::http::CancelledError&) {
    throw;
} catch (const platform::http::TransportError&) {
    throw ApiError{503, "customer_directory_unavailable", "Не удалось получить карточку клиента"};
} catch (const platform::http::ContractError&) {
    throw ApiError{
        503, "customer_directory_unavailable", "Некорректный ответ справочника контрагентов"};
}
}  // namespace shipments::helpers
