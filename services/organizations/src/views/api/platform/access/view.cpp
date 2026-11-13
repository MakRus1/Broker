#include <components/access.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/platform/access/view.hpp>

namespace organizations::views::api::platform::access {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = ::platform::auth::GetIdentity(context);

    auto result = deps.access->ResolvePlatformAccess(request.body, identity);

    return Response200{
        .permissions = std::move(result.permissions), .revision = std::move(result.revision)};
}
}  // namespace organizations::views::api::platform::access
