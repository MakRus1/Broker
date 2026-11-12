#include <components/sales_registry.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/sales/cards/filter_values/view.hpp>

namespace sales::views::api::v1::sales::cards::filter_values {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));

    const auto result = deps.registry->GetCardsFilterValues(organization, request.body);
    return Response200{
        .fields = result.fields, .next_after = result.next_after, .values = result.values};
}
}  // namespace sales::views::api::v1::sales::cards::filter_values
