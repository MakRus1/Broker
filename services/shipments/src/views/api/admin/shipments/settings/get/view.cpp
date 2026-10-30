#include <helpers/business_rules.hpp>
#include <platform/auth/organization_admin.hpp>
#include <views/api/admin/shipments/settings/get/view.hpp>

namespace shipments::views::api::admin::shipments::settings::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    try {
        platform::auth::RequireOrganizationAdministrator(
            deps.organizations, context, request.body.organization_id);

        const auto result = GetBusinessSettings(deps, request.body.organization_id);
        return Response200{.allowed_container_types = result.allowed_container_types,
            .allowed_route_types = result.allowed_route_types,
            .planned_containers_allowed = result.planned_containers_allowed,
            .revision = result.revision,
            .rules = result.rules};
    } catch (const platform::http::ErrorByStatus<403>&) {
        return Response403{.error = "forbidden", .message = "Нет права управления организацией"};
    } catch (const platform::http::ErrorByStatus<404>&) {
        return Response404{.error = "not_found", .message = "Организация не найдена"};
    } catch (const platform::http::ApiError&) {
        return Response503{
            .error = "organizations_unavailable", .message = "Не удалось проверить доступ"};
    } catch (const platform::http::CancelledError&) {
        throw;
    } catch (const platform::http::TransportError&) {
        return Response503{
            .error = "organizations_unavailable", .message = "Не удалось проверить доступ"};
    } catch (const platform::http::ContractError&) {
        return Response503{
            .error = "organizations_unavailable", .message = "Некорректный ответ проверки доступа"};
    } catch (const ::shipments::ApiError& error) {
        if (error.Status == 400)
            return Response400{.error = error.Code, .message = error.what()};
        return Response503{.error = error.Code, .message = error.what()};
    }
}
}  // namespace shipments::views::api::admin::shipments::settings::get
