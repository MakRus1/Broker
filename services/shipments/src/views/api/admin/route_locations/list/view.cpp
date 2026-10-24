#include <helpers/converters/route_details.hpp>
#include <helpers/database.hpp>
#include <platform/auth/identity.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/utils/text.hpp>
#include <views/api/admin/route_locations/list/view.hpp>

namespace shipments::views::api::admin::route_locations::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    if (!platform::auth::GetIdentity(context).Permissions.contains("platform.admin"))
        return Response403{
            .error = "platform_admin_required", .message = "Нужны права администратора платформы"};
    const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster,
        sql::kListRouteLocations, models::EnumValue(request.body.kind),
        userver::utils::text::Trim(request.body.query.value_or("")),
        request.body.after.has_value()
            ? std::optional<boost::uuids::uuid>{boost::uuids::string_generator{}(
                  request.body.after.value())}
            : std::nullopt);
    Response200 result;
    for (const auto& row : rows.AsSetOf<pgtypes::TransportLocationV1>()) {
        if (result.items.size() == 50) {
            result.next = result.items.back().id;
            break;
        }
        result.items.push_back(Convert(row));
    }
    return result;
}
}  // namespace shipments::views::api::admin::route_locations::list
