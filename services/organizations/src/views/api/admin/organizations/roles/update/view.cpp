#include <components/roles.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/admin/organizations/roles/update/view.hpp>

namespace organizations::views::api::admin::organizations::roles::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = ::platform::auth::GetIdentity(context);

    auto result = deps.roles->UpdateRole(request.body, identity);

    return Response200{.role = std::move(result.role)};
}
}  // namespace organizations::views::api::admin::organizations::roles::update
