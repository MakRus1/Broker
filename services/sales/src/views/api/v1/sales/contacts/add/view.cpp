#include <components/contacts.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/sales/contacts/add/view.hpp>

namespace sales::views::api::v1::sales::contacts::add {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& actor = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(actor);

    const auto result = deps.contacts->AddContact(organization, actor, request.body);
    return Response200{.card = result.card, .item = result.item};
}
}  // namespace sales::views::api::v1::sales::contacts::add
