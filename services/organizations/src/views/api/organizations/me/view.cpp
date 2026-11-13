#include <components/access.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/organizations/me/view.hpp>

namespace organizations::views::api::organizations::me {
Response View::Handle(Request&&, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = ::platform::auth::GetIdentity(context);

    auto result = deps.access->FindMyOrganizations(identity);

    return Response200{.managed_organization_ids = std::move(result.managed_organization_ids),
        .organizations = std::move(result.organizations),
        .platform_admin = std::move(result.platform_admin)};
}
}  // namespace organizations::views::api::organizations::me
