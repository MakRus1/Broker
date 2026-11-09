#include <components/members.hpp>
#include <utility>
#include <views/api/admin/organizations/members/create/view.hpp>

namespace organizations::views::api::admin::organizations::members::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.members->Create(request.body, context);
    return Response201{.member = std::move(result.member)};
}
}  // namespace organizations::views::api::admin::organizations::members::create
