#pragma once
#include <boost/uuid/uuid.hpp>
#include <platform/auth/identity.hpp>

namespace platform::auth {
// Проверяет управление именно указанной организацией, включая корневого администратора.
// Выполняется перед чтением данных и до открытия локальной транзакции.
template <typename OrganizationsClient>
void RequireOrganizationAdministrator(const OrganizationsClient& organizations,
    const userver::server::request::RequestContext& context,
    const boost::uuids::uuid& organization) {
    organizations.ApiOrganizationsAdministrationCheck(
        {.body = {.organization_id = organization}}, GetAccessToken(context));
}

}  // namespace platform::auth
