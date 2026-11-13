#include <components/cards.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/sales/cards/history/view.hpp>

namespace sales::views::api::v1::sales::cards::history {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& actor = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(actor);

    return Response200{.items = deps.cards->GetCardHistory(organization, request.body)};
}
}  // namespace sales::views::api::v1::sales::cards::history
