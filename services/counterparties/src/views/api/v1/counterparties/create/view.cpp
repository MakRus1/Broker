#include <components/counterparties.hpp>
#include <converters/counterparties.hpp>
#include <helpers/counterparty_validation.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/counterparties/create/view.hpp>

namespace counterparties::views::api::v1::counterparties::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    platform::auth::CheckPermission(context, permissions::COUNTERPARTIES_WRITE);
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);

    const auto value = helpers::NormalizeAndValidateCounterparty(converters::Convert(request.body));
    if (request.body.logist.has_value())
        deps.counterparties->ValidateLogist(
            organization, platform::auth::GetAccessToken(context), request.body.logist.value());

    auto result =
        deps.counterparties->CreateCounterparty(organization, identity, request.body, value);

    return Response201{.counterparty = std::move(result)};
}
}  // namespace counterparties::views::api::v1::counterparties::create
