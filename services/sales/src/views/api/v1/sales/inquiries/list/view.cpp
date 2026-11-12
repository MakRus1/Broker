#include <components/sales_registry.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/sales/inquiries/list/view.hpp>

namespace sales::views::api::v1::sales::inquiries::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));

    const auto result = deps.registry->ListInquiries(organization, request.body);
    return Response200{.items = result.items, .next_cursor = result.next_cursor};
}
}  // namespace sales::views::api::v1::sales::inquiries::list
