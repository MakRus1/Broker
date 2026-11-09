#include <components/access.hpp>
#include <utility>
#include <views/api/organizations/membership/get/view.hpp>

namespace organizations::views::api::organizations::membership::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.access->Membership(request.body, context);
    return Response200{.organization_id = std::move(result.organization_id)};
}
}  // namespace organizations::views::api::organizations::membership::get
