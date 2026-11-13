#include <components/permission_releases.hpp>
#include <utility>
#include <views/api/internal/permissions/releases/upsert/view.hpp>

namespace organizations::views::api::internal::permissions::releases::upsert {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {

    auto result = deps.permission_releases->RegisterPermissionRelease(request.body);

    return Response200{.success = std::move(result.success)};
}
}  // namespace organizations::views::api::internal::permissions::releases::upsert
