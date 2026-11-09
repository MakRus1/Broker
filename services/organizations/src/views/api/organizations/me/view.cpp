#include <components/access.hpp>
#include <utility>
#include <views/api/organizations/me/view.hpp>

namespace organizations::views::api::organizations::me {
Response View::Handle(Request&&, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.access->Mine(context);
    return Response200{.managed_organization_ids = std::move(result.managed_organization_ids),
        .organizations = std::move(result.organizations),
        .platform_admin = std::move(result.platform_admin)};
}
}  // namespace organizations::views::api::organizations::me
