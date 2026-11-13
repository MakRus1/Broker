#include <components/settings.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/sales/settings/view.hpp>

namespace sales::views::api::v1::sales::settings {
Response View::Handle(Request&&, userver::server::request::RequestContext& context, Deps deps) {
    const auto result = deps.settings->GetSettings(
        platform::auth::GetOrganization(platform::auth::GetIdentity(context)));
    return Response200{.inquiry_number_template = result.inquiry_number_template,
        .reset_number_yearly = result.reset_number_yearly,
        .revision = result.revision,
        .stages = result.stages};
}
}  // namespace sales::views::api::v1::sales::settings
