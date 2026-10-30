#include <helpers/access.hpp>
#include <helpers/admin_registry.hpp>
#include <helpers/audit.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/utils/text.hpp>
#include <views/api/admin/organizations/list/view.hpp>

namespace organizations::views::api::admin::organizations::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::organizations::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    try {
        RequirePlatformAdmin(identity);
        const auto scope = registry::CursorScope(request.body, "organizations");
        if (!registry::CursorMatches(request.body, scope, EnumValue(request.body.sort) == "id"))
            throw ApiError<400>{
                "invalid_cursor", "Курсор не соответствует фильтрам или сортировке"};
        const auto query = OrganizationQuery(request.body);
        auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster,
            pg::TransactionOptions{
                pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
        transaction.Execute(sql::kRegistryPlan);
        auto rows =
            transaction.Execute(query.Statement, query.Parameters)
                .AsContainer<std::vector<registry::Row<pgtypes::OrganizationV1>>>(pg::kRowTag);
        const bool more = rows.size() > static_cast<std::size_t>(request.body.limit);
        if (more)
            rows.pop_back();
        std::optional<models::OrganizationListCursor> next;
        if (more)
            next =
                models::OrganizationListCursor{.id = boost::uuids::to_string(rows.back().Value.Id),
                    .scope = scope,
                    .value = rows.back().SortKey};
        const auto items =
            registry::ConvertItems(rows, [](const auto& row) { return Convert(row); });
        transaction.Commit();
        return Response200{.items = items, .next_cursor = next};
    } catch (const userver::storages::postgres::UniqueViolation&) {
        return Response409{.error = "already_exists", .message = "Такая запись уже существует"};
    }
}
}  // namespace organizations::views::api::admin::organizations::list
