#include <components/access.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/organizations/membership/get/view.hpp>

namespace organizations::views::api::organizations::membership::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = ::platform::auth::GetIdentity(context);

    auto result = deps.access->CheckMembership(request.body, identity);

    return Response200{.organization_id = std::move(result.organization_id)};
}
}  // namespace organizations::views::api::organizations::membership::get
