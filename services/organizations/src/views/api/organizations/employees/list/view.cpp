#include <helpers/access.hpp>
#include <helpers/converters.hpp>
#include <permissions/permissions.hpp>
#include <views/api/organizations/employees/list/view.hpp>

namespace organizations::views::api::organizations::employees::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::organizations::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    const auto access = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kResolveAccess,
        identity.Issuer, identity.Subject, request.body.organization_id);
    if (access.Size() != 1)
        return Response403{.error = "organization_access_denied",
            .message = "Нет доступа к сотрудникам организации"};
    const auto granted = access.AsSingleRow<AccessRow>(pg::kRowTag);
    const std::set<std::string> permissions(granted.Permissions.begin(), granted.Permissions.end());
    if (!permissions.contains(std::string{::permissions::ORGANIZATIONS_READ_EMPLOYEES}))
        return Response403{.error = "permission_denied",
            .message = "Недостаточно прав для просмотра справочника сотрудников"};
    const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kSlave, sql::kListEmployees,
        granted.Organization, request.body.subject, request.body.query, request.body.limit);
    return Response200{.items = ConvertRows<pgtypes::OrganizationEmployeeV1>(rows)};
}
}  // namespace organizations::views::api::organizations::employees::list
