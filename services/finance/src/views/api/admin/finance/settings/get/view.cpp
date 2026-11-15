#include <components/settings.hpp>
#include <models/errors.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <platform/auth/organization_admin.hpp>
#include <platform/http_client.hpp>
#include <utility>
#include <views/api/admin/finance/settings/get/view.hpp>

namespace finance::views::api::admin::finance::settings::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    try {
        platform::auth::RequireOrganizationAdministrator(
            deps.organizations, context, request.body.organization_id);
    } catch (const platform::http::ErrorByStatus<403>&) {
        throw ApiError<403>{"forbidden", "Нет права управления организацией"};
    } catch (const platform::http::ErrorByStatus<404>&) {
        throw ApiError<404>{"organization_not_found", "Организация не найдена"};
    } catch (const platform::http::CancelledError&) {
        throw;
    } catch (const platform::http::ApiError&) {
        throw ApiError<503>{
            "organizations_unavailable", "Не удалось проверить доступ к организации"};
    } catch (const platform::http::TransportError&) {
        throw ApiError<503>{"organizations_unavailable", "Сервис организаций недоступен"};
    } catch (const platform::http::ContractError&) {
        throw ApiError<503>{"organizations_unavailable", "Некорректный ответ сервиса организаций"};
    }

    auto result = deps.settings->GetSettings(request.body.organization_id);

    return Response200{.incoming_number_template = std::move(result.incoming_number_template),
        .outgoing_number_template = std::move(result.outgoing_number_template),
        .reporting_currency = std::move(result.reporting_currency),
        .reset_numbers_yearly = std::move(result.reset_numbers_yearly),
        .revision = std::move(result.revision)};
}
}  // namespace finance::views::api::admin::finance::settings::get
