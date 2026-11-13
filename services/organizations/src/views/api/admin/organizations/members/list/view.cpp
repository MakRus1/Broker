#include <components/members.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/admin/organizations/members/list/view.hpp>

namespace organizations::views::api::admin::organizations::members::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = ::platform::auth::GetIdentity(context);

    auto result = deps.members->ListMembers(request.body, identity);

    return Response200{
        .items = std::move(result.items), .next_cursor = std::move(result.next_cursor)};
}
}  // namespace organizations::views::api::admin::organizations::members::list
