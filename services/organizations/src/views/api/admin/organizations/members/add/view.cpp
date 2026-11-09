#include <components/members.hpp>
#include <utility>
#include <views/api/admin/organizations/members/add/view.hpp>

namespace organizations::views::api::admin::organizations::members::add {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.members->Add(request.body, context);
    return Response201{.member = std::move(result.member)};
}
}  // namespace organizations::views::api::admin::organizations::members::add
