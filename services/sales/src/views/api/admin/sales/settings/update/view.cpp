#include <components/settings.hpp>
#include <models/errors.hpp>
#include <platform/auth/identity.hpp>
#include <platform/auth/organization_admin.hpp>
#include <platform/http_client.hpp>
#include <views/api/admin/sales/settings/update/view.hpp>

namespace sales::views::api::admin::sales::settings::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) try {
    platform::auth::RequireOrganizationAdministrator(
        deps.organizations, context, request.body.organization_id);

    const auto result = deps.settings->UpdateSettings(request.body);
    return Response200{.inquiry_number_template = result.inquiry_number_template,
        .reset_number_yearly = result.reset_number_yearly,
        .revision = result.revision,
        .stages = result.stages};
} catch (const platform::http::ErrorByStatus<403>&) {

    throw ApiError<403>{"reference_forbidden", "Нет доступа к связанным данным"};
} catch (const platform::http::ErrorByStatus<404>&) {
    throw ApiError<404>{"reference_not_found", "Связанный объект не найден в организации"};
}

}  // namespace sales::views::api::admin::sales::settings::update
