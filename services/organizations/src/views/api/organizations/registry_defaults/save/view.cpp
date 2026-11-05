#include <helpers/audit.hpp>
#include <helpers/registry_defaults.hpp>
#include <views/api/organizations/registry_defaults/save/view.hpp>

namespace organizations::views::api::organizations::registry_defaults::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::organizations::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    const auto& body = request.body;
    ValidateRegistryDefault(body.value);
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    RequireAdministrator(transaction, body.organization_id, identity);
    RequireRole(transaction, body.organization_id, body.role);
    const auto rows =
        transaction.Execute(body.expected_revision == 0 ? sql::kSaveRoleRegistryDefault
                                                        : sql::kUpdateRoleRegistryDefault,
            body.organization_id, body.role, body.registry, body.value, body.expected_revision);
    if (rows.IsEmpty())
        throw ApiError<409>{"registry_default_changed", "Вид роли уже изменён. Обновите данные"};
    const auto item = ConvertRegistryDefault(rows.AsSingleRow<RegistryDefaultRow>(pg::kRowTag));
    WriteAudit(transaction, body.organization_id, identity, "role.registry_default.updated", item);
    transaction.Commit();
    return Response200{.item = item};
}
}  // namespace organizations::views::api::organizations::registry_defaults::save
