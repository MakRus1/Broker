#include <components/organizations.hpp>
#include <helpers/access.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/admin/organizations/create/view.hpp>

namespace organizations::views::api::admin::organizations::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = ::platform::auth::GetIdentity(context);
    helpers::RequirePlatformAdmin(identity);

    auto result = deps.organizations->CreateOrganization(request.body, identity);

    return Response201{.organization = std::move(result.organization)};
}
}  // namespace organizations::views::api::admin::organizations::create
