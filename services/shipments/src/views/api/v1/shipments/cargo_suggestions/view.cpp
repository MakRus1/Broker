#include <components/cargo_suggestions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/shipments/cargo_suggestions/view.hpp>

namespace shipments::views::api::v1::shipments::cargo_suggestions {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    return Response200{.items = deps.cargo_suggestions->FindSuggestions(organization,
                           request.body.customer_id, request.body.query.value_or(""))};
}
}  // namespace shipments::views::api::v1::shipments::cargo_suggestions
