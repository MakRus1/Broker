#include <components/members.hpp>
#include <utility>
#include <views/api/admin/organizations/members/update_profile/view.hpp>

namespace organizations::views::api::admin::organizations::members::update_profile {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.members->UpdateProfile(request.body, context);
    return Response200{.member = std::move(result.member)};
}
}  // namespace organizations::views::api::admin::organizations::members::update_profile
