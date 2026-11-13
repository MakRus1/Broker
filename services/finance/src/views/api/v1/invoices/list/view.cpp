#include <components/invoices.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/invoices/list/view.hpp>

namespace finance::views::api::v1::invoices::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_INVOICES);

    auto result = deps.invoices->ListInvoices(organization, request.body);
    return Response200{
        .items = std::move(result.items), .next_cursor = std::move(result.next_cursor)};
}
}  // namespace finance::views::api::v1::invoices::list
