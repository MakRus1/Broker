#include <components/roles.hpp>
#include <utility>
#include <views/api/admin/organizations/roles/list/view.hpp>

namespace organizations::views::api::admin::organizations::roles::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.roles->List(request.body, context);
    return Response200{
        .items = std::move(result.items), .permissions = std::move(result.permissions)};
}
}  // namespace organizations::views::api::admin::organizations::roles::list
