#include <components/members.hpp>
#include <utility>
#include <views/api/admin/organizations/members/update/view.hpp>

namespace organizations::views::api::admin::organizations::members::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.members->Update(request.body, context);
    return Response200{.member = std::move(result.member)};
}
}  // namespace organizations::views::api::admin::organizations::members::update
