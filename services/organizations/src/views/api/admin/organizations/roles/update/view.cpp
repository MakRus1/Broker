#include <components/roles.hpp>
#include <utility>
#include <views/api/admin/organizations/roles/update/view.hpp>

namespace organizations::views::api::admin::organizations::roles::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.roles->Update(request.body, context);
    return Response200{.role = std::move(result.role)};
}
}  // namespace organizations::views::api::admin::organizations::roles::update
