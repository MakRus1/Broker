#include <components/locations.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/admin/route_locations/save/view.hpp>

namespace transport_locations::views::api::admin::route_locations::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    if (!platform::auth::GetIdentity(context).Permissions.contains("platform.admin"))
        return Response403{
            .error = "platform_admin_required", .message = "Нужны права администратора платформы"};
    auto result = deps.locations->Save(request.body);
    return Response200{.code = std::move(result.code),
        .country_code = std::move(result.country_code),
        .id = std::move(result.id),
        .kind = std::move(result.kind),
        .name = std::move(result.name)};
}
}  // namespace transport_locations::views::api::admin::route_locations::save
