#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <platform/auth/identity.hpp>
#include <userver/storages/postgres/exceptions.hpp>
#include <userver/utils/text.hpp>
#include <views/api/admin/route_locations/save/view.hpp>

namespace transport_locations::views::api::admin::route_locations::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::transport_locations::helpers;
    if (!platform::auth::GetIdentity(context).Permissions.contains("platform.admin"))
        return Response403{
            .error = "platform_admin_required", .message = "Нужны права администратора платформы"};
    try {
        const auto name = userver::utils::text::Trim(request.body.name);
        if (name.empty())
            return Response400{.error = "invalid_name", .message = "Укажите название"};
        auto code = request.body.code;
        if (code.has_value()) {
            code = userver::utils::text::Trim(code.value());
            if (code.value().empty())
                code.reset();
        }
        const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster,
            sql::kSaveRouteLocation, request.body.id, models::EnumValue(request.body.kind), name,
            request.body.country_code, code, request.body.existing.value_or(false));
        if (rows.IsEmpty())
            return Response409{.error = "location_exists",
                .message = "Запись уже существует или имеет другой тип"};
        const auto value = Convert(rows.AsSingleRow<pgtypes::TransportLocationV1>());
        return Response200{.code = value.code,
            .country_code = value.country_code,
            .id = value.id,
            .kind = value.kind,
            .name = value.name};
    } catch (const userver::storages::postgres::UniqueViolation&) {
        return Response409{
            .error = "location_code_exists", .message = "Код уже используется в этой стране"};
    }
}
}  // namespace transport_locations::views::api::admin::route_locations::save
