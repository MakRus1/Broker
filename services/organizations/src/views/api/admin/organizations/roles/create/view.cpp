#include <components/roles.hpp>
#include <utility>
#include <views/api/admin/organizations/roles/create/view.hpp>

namespace organizations::views::api::admin::organizations::roles::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.roles->Create(request.body, context);
    return Response201{.role = std::move(result.role)};
}
}  // namespace organizations::views::api::admin::organizations::roles::create
