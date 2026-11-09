#include <components/organizations.hpp>
#include <utility>
#include <views/api/admin/organizations/create/view.hpp>

namespace organizations::views::api::admin::organizations::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.organizations->Create(request.body, context);
    return Response201{.organization = std::move(result.organization)};
}
}  // namespace organizations::views::api::admin::organizations::create
