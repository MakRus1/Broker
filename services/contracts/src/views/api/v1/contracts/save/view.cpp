#include <components/contract_card.hpp>
#include <components/contracts.hpp>
#include <converters/contracts.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/contracts/save/view.hpp>

namespace contracts::views::api::v1::contracts::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    platform::auth::CheckPermission(context, permissions::CONTRACTS_WRITE);
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);

    auto body = request.body;
    contract_card::NormalizeAndValidateContract(body);
    auto value = converters::Convert(body);

    deps.contracts->ValidateContractParties(platform::auth::GetAccessToken(context), body);

    auto result = deps.contracts->SaveContract(
        organization, identity, request.body.expected_revision, std::move(value));

    return Response200{.contract = std::move(result)};
}
}  // namespace contracts::views::api::v1::contracts::save
