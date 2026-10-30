#include <helpers/access.hpp>
#include <helpers/audit.hpp>
#include <helpers/converters.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/utils/text.hpp>
#include <views/api/organizations/access/view.hpp>

namespace organizations::views::api::organizations::access {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::organizations::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    try {
        const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster,
            sql::kResolveAccess, identity.Issuer, identity.Subject, request.body.organization_id);
        if (rows.Size() != 1)
            throw ApiError<403>{"organization_access_denied", "Выберите доступную организацию"};
        const auto [organization, granted] = rows.AsSingleRow<AccessRow>(pg::kRowTag);
        const std::set<std::string> permissions(granted.begin(), granted.end());
        if (!permissions.contains(request.body.permission))
            throw ApiError<403>{"permission_denied", "Недостаточно прав в организации"};
        return Response200{.organization_id = organization,
            .permissions = {permissions.begin(), permissions.end()}};
    } catch (const userver::storages::postgres::UniqueViolation&) {
        return Response409{.error = "already_exists", .message = "Такая запись уже существует"};
    }
}
}  // namespace organizations::views::api::organizations::access
