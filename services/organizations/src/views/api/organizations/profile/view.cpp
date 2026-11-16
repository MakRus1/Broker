#include <components/members.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/organizations/profile/view.hpp>

namespace organizations::views::api::organizations::profile {
Response View::Handle(Request&&, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = ::platform::auth::GetIdentity(context);

    return Response200{.display_name = deps.members->FindProfileDisplayName(identity)};
}
}  // namespace organizations::views::api::organizations::profile
