#include <components/contacts.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/sales/contacts/update/view.hpp>

namespace sales::views::api::v1::sales::contacts::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& actor = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(actor);

    return Response200{.item = deps.contacts->UpdateContact(organization, actor, request.body)};
}
}  // namespace sales::views::api::v1::sales::contacts::update
