#include <components/service_charges.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/service_charges/filter_values/view.hpp>

namespace finance::views::api::v1::service_charges::filter_values {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    auto result =
        deps.service_charges->GetServiceChargeFilterValues(organization, std::move(request.body));
    return Response200{.fields = std::move(result.fields),
        .next_after = std::move(result.next_after),
        .values = std::move(result.values)};
}
}  // namespace finance::views::api::v1::service_charges::filter_values
