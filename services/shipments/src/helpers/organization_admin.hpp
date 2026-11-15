#pragma once
#include <clients/organizations/client.hpp>
#include <models/errors.hpp>
#include <platform/auth/organization_admin.hpp>

namespace shipments::helpers {
inline void RequireOrganizationAdministrator(const clients::organizations::Client& organizations,
    const userver::server::request::RequestContext& context,
    const boost::uuids::uuid& organization) try {
    platform::auth::RequireOrganizationAdministrator(organizations, context, organization);
} catch (const platform::http::ErrorByStatus<403>&) {

    throw ApiError<403>{"forbidden", "Нет права управления организацией"};
} catch (const platform::http::ErrorByStatus<404>&) {
    throw ApiError<404>{"not_found", "Организация не найдена"};
} catch (const platform::http::ApiError&) {
    throw ApiError<503>{"organizations_unavailable", "Не удалось проверить доступ"};
} catch (const platform::http::CancelledError&) {
    throw;
} catch (const platform::http::TransportError&) {
    throw ApiError<503>{"organizations_unavailable", "Не удалось проверить доступ"};
} catch (const platform::http::ContractError&) {
    throw ApiError<503>{"organizations_unavailable", "Некорректный ответ проверки доступа"};
}
}  // namespace shipments::helpers
