#include <components/cargo_suggestions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/shipments/cargo_suggestions/remove/view.hpp>

namespace shipments::views::api::v1::shipments::cargo_suggestions::remove {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    return Response200{.deleted = deps.cargo_suggestions->RemoveSuggestion(
                           organization, request.body.customer_id, request.body.value)};
}
}  // namespace shipments::views::api::v1::shipments::cargo_suggestions::remove
