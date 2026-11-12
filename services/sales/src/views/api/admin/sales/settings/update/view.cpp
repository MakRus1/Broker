#include <components/sales_directory.hpp>
#include <components/settings.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/admin/sales/settings/update/view.hpp>

namespace sales::views::api::admin::sales::settings::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    deps.directory->RequireAdministrator(
        request.body.organization_id, platform::auth::GetAccessToken(context));

    const auto result = deps.settings->UpdateSettings(request.body);
    return Response200{.inquiry_number_template = result.inquiry_number_template,
        .reset_number_yearly = result.reset_number_yearly,
        .revision = result.revision,
        .stages = result.stages};
}
}  // namespace sales::views::api::admin::sales::settings::update
