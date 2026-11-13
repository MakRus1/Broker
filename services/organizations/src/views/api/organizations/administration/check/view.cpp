#include <components/access.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/organizations/administration/check/view.hpp>

namespace organizations::views::api::organizations::administration::check {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = ::platform::auth::GetIdentity(context);

    auto result = deps.access->CheckAdministrator(request.body, identity);

    return Response200{.organization_id = std::move(result.organization_id)};
}
}  // namespace organizations::views::api::organizations::administration::check
