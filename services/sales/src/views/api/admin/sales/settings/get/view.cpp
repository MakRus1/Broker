#include <components/settings.hpp>
#include <models/errors.hpp>
#include <platform/auth/identity.hpp>
#include <platform/auth/organization_admin.hpp>
#include <platform/http_client.hpp>
#include <views/api/admin/sales/settings/get/view.hpp>

namespace sales::views::api::admin::sales::settings::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) try {
    platform::auth::RequireOrganizationAdministrator(
        deps.organizations, context, request.body.organization_id);

    const auto result = deps.settings->GetSettings(request.body.organization_id);
    return Response200{.inquiry_number_template = result.inquiry_number_template,
        .reset_number_yearly = result.reset_number_yearly,
        .revision = result.revision,
        .stages = result.stages};
} catch (const platform::http::ErrorByStatus<403>&) {

    throw ApiError<403>{"reference_forbidden", "Нет доступа к связанным данным"};
} catch (const platform::http::ErrorByStatus<404>&) {
    throw ApiError<404>{"reference_not_found", "Связанный объект не найден в организации"};
} catch (const platform::http::CancelledError&) {
    throw;
} catch (const platform::http::ApiError&) {
    throw ApiError<502>{"reference_unavailable", "Не удалось получить связанные данные"};
} catch (const platform::http::TransportError&) {
    throw ApiError<502>{"reference_unavailable", "Сервис связанных данных недоступен"};
} catch (const platform::http::ContractError&) {
    throw ApiError<502>{"reference_unavailable", "Некорректный ответ сервиса связанных данных"};
}

}  // namespace sales::views::api::admin::sales::settings::get
