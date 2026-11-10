#include <components/railway_stations.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/admin/railway_stations/list/view.hpp>

namespace transport_locations::views::api::admin::railway_stations::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    if (!platform::auth::GetIdentity(context).Permissions.contains("platform.admin"))
        return Response403{
            .error = "platform_admin_required", .message = "Нужны права администратора платформы"};
    auto result = deps.stations->List(request.body);
    return Response200{.items = std::move(result.items),
        .next = std::move(result.next),
        .next_cursor = std::move(result.next_cursor)};
}
}  // namespace transport_locations::views::api::admin::railway_stations::list
