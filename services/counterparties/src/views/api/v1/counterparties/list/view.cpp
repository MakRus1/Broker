#include <components/counterparty_registry.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/counterparties/list/view.hpp>

namespace counterparties::views::api::v1::counterparties::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    platform::auth::CheckPermission(context, permissions::COUNTERPARTIES_READ);
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);

    auto result = deps.registry->ListCounterparties(organization, request.body);

    return Response200{
        .items = std::move(result.items), .next_cursor = std::move(result.next_cursor)};
}
}  // namespace counterparties::views::api::v1::counterparties::list
