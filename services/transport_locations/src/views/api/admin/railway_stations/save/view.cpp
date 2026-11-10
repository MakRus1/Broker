#include <components/railway_stations.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/admin/railway_stations/save/view.hpp>

namespace transport_locations::views::api::admin::railway_stations::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    if (!platform::auth::GetIdentity(context).Permissions.contains("platform.admin"))
        return Response403{
            .error = "platform_admin_required", .message = "Нужны права администратора платформы"};
    auto result = deps.stations->Save(request.body);
    return Response200{.country_code = std::move(result.country_code),
        .esr_code = std::move(result.esr_code),
        .esr_source = std::move(result.esr_source),
        .id = std::move(result.id),
        .name = std::move(result.name),
        .uic_code = std::move(result.uic_code)};
}
}  // namespace transport_locations::views::api::admin::railway_stations::save
