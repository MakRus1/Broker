#include <components/contacts.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/sales/contacts/history/view.hpp>

namespace sales::views::api::v1::sales::contacts::history {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& actor = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(actor);

    return Response200{.items = deps.contacts->GetContactHistory(organization, request.body)};
}
}  // namespace sales::views::api::v1::sales::contacts::history
