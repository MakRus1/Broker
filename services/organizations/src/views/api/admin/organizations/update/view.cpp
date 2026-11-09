#include <components/organizations.hpp>
#include <utility>
#include <views/api/admin/organizations/update/view.hpp>

namespace organizations::views::api::admin::organizations::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.organizations->Update(request.body, context);
    return Response200{.organization = std::move(result.organization)};
}
}  // namespace organizations::views::api::admin::organizations::update
