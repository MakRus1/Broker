#include <components/roles.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/admin/organizations/roles/list/view.hpp>

namespace organizations::views::api::admin::organizations::roles::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = ::platform::auth::GetIdentity(context);

    auto result = deps.roles->ListRoles(request.body, identity);

    return Response200{
        .items = std::move(result.items), .permissions = std::move(result.permissions)};
}
}  // namespace organizations::views::api::admin::organizations::roles::list
