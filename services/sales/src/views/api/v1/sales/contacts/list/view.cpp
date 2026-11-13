#include <components/contacts.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/sales/contacts/list/view.hpp>

namespace sales::views::api::v1::sales::contacts::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& actor = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(actor);

    const auto result = deps.contacts->ListContacts(organization, request.body);
    return Response200{.items = result.items, .next_before = result.next_before};
}
}  // namespace sales::views::api::v1::sales::contacts::list
