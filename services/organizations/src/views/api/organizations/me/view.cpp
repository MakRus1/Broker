#include <helpers/access.hpp>
#include <helpers/audit.hpp>
#include <helpers/converters.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/utils/text.hpp>
#include <views/api/organizations/me/view.hpp>

namespace organizations::views::api::organizations::me {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::organizations::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    try {
        (void)request;
        const auto rows = deps.pg_cluster->Execute(
            pg::ClusterHostType::kMaster, sql::kMyOrganizations, identity.Issuer, identity.Subject);
        const auto managed = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster,
            sql::kManagedOrganizations, identity.Issuer, identity.Subject);
        return Response200{
            .managed_organization_ids = managed.AsContainer<std::vector<std::string>>(),
            .organizations = ConvertRows<pgtypes::OrganizationV1>(rows),
            .platform_admin = IsPlatformAdmin(identity)};
    } catch (const ::organizations::ApiError& error) {
        switch (error.Status) {
        case 400:
            return Response400{.error = error.Code, .message = error.what()};
        case 403:
            return Response403{.error = error.Code, .message = error.what()};
        case 404:
            return Response404{.error = error.Code, .message = error.what()};
        case 409:
            return Response409{.error = error.Code, .message = error.what()};
        default:
            return Response503{.error = error.Code, .message = error.what()};
        }
    } catch (const userver::storages::postgres::UniqueViolation&) {
        return Response409{.error = "already_exists", .message = "Такая запись уже существует"};
    }
}
}  // namespace organizations::views::api::organizations::me
