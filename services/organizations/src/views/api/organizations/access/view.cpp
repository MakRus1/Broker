#include <components/access.hpp>
#include <utility>
#include <views/api/organizations/access/view.hpp>

namespace organizations::views::api::organizations::access {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.access->ResolveOrganization(request.body, context);
    return Response200{.organization_id = std::move(result.organization_id),
        .permissions = std::move(result.permissions)};
}
}  // namespace organizations::views::api::organizations::access
