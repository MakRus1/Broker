#include <components/counterparties.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/counterparties/update/view.hpp>

namespace counterparties::views::api::v1::counterparties::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    platform::auth::CheckPermission(context, permissions::COUNTERPARTIES_WRITE);
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);

    if (request.body.logist.has_value())
        deps.counterparties->ValidateLogist(
            organization, platform::auth::GetAccessToken(context), request.body.logist.value());

    auto result = deps.counterparties->UpdateCounterparty(organization, identity, request.body);

    return Response200{.counterparty = std::move(result)};
}
}  // namespace counterparties::views::api::v1::counterparties::update
