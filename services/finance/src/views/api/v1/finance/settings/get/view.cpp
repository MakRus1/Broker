#include <components/settings.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/finance/settings/get/view.hpp>

namespace finance::views::api::v1::finance::settings::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);

    auto result = deps.settings->GetSettings(organization);
    return Response200{.incoming_number_template = std::move(result.incoming_number_template),
        .outgoing_number_template = std::move(result.outgoing_number_template),
        .reporting_currency = std::move(result.reporting_currency),
        .reset_numbers_yearly = std::move(result.reset_numbers_yearly),
        .revision = std::move(result.revision)};
}
}  // namespace finance::views::api::v1::finance::settings::get
