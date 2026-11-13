#include <components/organizations.hpp>
#include <helpers/access.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/admin/organizations/list/view.hpp>

namespace organizations::views::api::admin::organizations::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = ::platform::auth::GetIdentity(context);
    helpers::RequirePlatformAdmin(identity);

    auto result = deps.organizations->ListOrganizations(request.body);

    return Response200{
        .items = std::move(result.items), .next_cursor = std::move(result.next_cursor)};
}
}  // namespace organizations::views::api::admin::organizations::list
