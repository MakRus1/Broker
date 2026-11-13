#include <components/access.hpp>
#include <helpers/access.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/admin/platform_access/get/view.hpp>

namespace organizations::views::api::admin::platform_access::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = ::platform::auth::GetIdentity(context);
    helpers::RequirePlatformAdmin(identity);

    auto result = deps.access->GetPlatformAccess(request.body, identity);

    return Response200{.access = std::move(result.access), .catalog = std::move(result.catalog)};
}
}  // namespace organizations::views::api::admin::platform_access::get
