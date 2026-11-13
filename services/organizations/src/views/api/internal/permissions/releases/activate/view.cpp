#include <components/permission_releases.hpp>
#include <utility>
#include <views/api/internal/permissions/releases/activate/view.hpp>

namespace organizations::views::api::internal::permissions::releases::activate {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {

    auto result = deps.permission_releases->ActivatePermissionRelease(request.body);

    return Response200{.success = std::move(result.success)};
}
}  // namespace organizations::views::api::internal::permissions::releases::activate
