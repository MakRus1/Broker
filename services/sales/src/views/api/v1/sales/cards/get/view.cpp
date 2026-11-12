#include <components/cards.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/sales/cards/get/view.hpp>

namespace sales::views::api::v1::sales::cards::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& actor = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(actor);

    return Response200{.item = deps.cards->FindCard(organization, request.body.id)};
}
}  // namespace sales::views::api::v1::sales::cards::get
