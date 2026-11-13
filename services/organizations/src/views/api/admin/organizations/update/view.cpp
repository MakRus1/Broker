#include <components/organizations.hpp>
#include <helpers/access.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/admin/organizations/update/view.hpp>

namespace organizations::views::api::admin::organizations::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = ::platform::auth::GetIdentity(context);
    helpers::RequirePlatformAdmin(identity);

    auto result = deps.organizations->UpdateOrganization(request.body, identity);

    return Response200{.organization = std::move(result.organization)};
}
}  // namespace organizations::views::api::admin::organizations::update
