#include <components/locations.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/admin/route_locations/archive/view.hpp>

namespace transport_locations::views::api::admin::route_locations::archive {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    if (!platform::auth::GetIdentity(context).Permissions.contains("platform.admin"))
        return Response403{
            .error = "platform_admin_required", .message = "Нужны права администратора платформы"};
    deps.locations->Archive(request.body.id);
    return Response200{.id = request.body.id};
}
}  // namespace transport_locations::views::api::admin::route_locations::archive
