#include <components/access.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/organizations/employees/list/view.hpp>

namespace organizations::views::api::organizations::employees::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = ::platform::auth::GetIdentity(context);

    auto result = deps.access->ListEmployees(request.body, identity);

    return Response200{.items = std::move(result.items)};
}
}  // namespace organizations::views::api::organizations::employees::list
