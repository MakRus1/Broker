#pragma once
#include <platform/auth/identity.hpp>
#include <userver/utils/boost_uuid4.hpp>

namespace platform::auth {
// Проверяет управление именно указанной организацией, включая корневого администратора.
// Выполняется перед чтением данных и до открытия локальной транзакции.
template <typename OrganizationsClient>
void RequireOrganizationAdministrator(const OrganizationsClient& organizations,
    const userver::server::request::RequestContext& context, const std::string& organization) {
    organizations.ApiOrganizationsAdministrationCheck(
        {.body = {.organization_id = userver::utils::BoostUuidFromString(organization)}},
        GetAccessToken(context));
}
}  // namespace platform::auth
