#include <components/members.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/admin/organizations/members/add/view.hpp>

namespace organizations::views::api::admin::organizations::members::add {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = ::platform::auth::GetIdentity(context);

    auto result = deps.members->AddExistingMember(request.body, identity);

    return Response201{.member = std::move(result.member)};
}
}  // namespace organizations::views::api::admin::organizations::members::add
