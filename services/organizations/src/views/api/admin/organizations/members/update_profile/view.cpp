#include <components/members.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/admin/organizations/members/update_profile/view.hpp>

namespace organizations::views::api::admin::organizations::members::update_profile {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = ::platform::auth::GetIdentity(context);

    auto result = deps.members->UpdateMemberProfile(request.body, identity);

    return Response200{.member = std::move(result.member)};
}
}  // namespace organizations::views::api::admin::organizations::members::update_profile
