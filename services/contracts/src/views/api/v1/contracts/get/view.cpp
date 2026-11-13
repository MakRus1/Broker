#include <components/contracts.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/contracts/get/view.hpp>

namespace contracts::views::api::v1::contracts::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    platform::auth::CheckPermission(context, permissions::CONTRACTS_READ);
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);

    auto result = deps.documents->FindContract(organization, request.body.id);

    return Response200{.contract = std::move(result)};
}
}  // namespace contracts::views::api::v1::contracts::get
