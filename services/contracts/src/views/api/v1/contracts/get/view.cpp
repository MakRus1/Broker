#include <helpers/contracts.hpp>
#include <permissions/permissions.hpp>
#include <views/api/v1/contracts/get/view.hpp>

namespace contracts::views::api::v1::contracts::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::contracts::helpers;

    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::CONTRACTS_READ);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto rows = deps.pg_cluster->Execute(
        pg::ClusterHostType::kMaster, sql::kGetContract, organization, request.body.id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Договор не найден"};
    return Response200{.contract = Convert(rows.AsSingleRow<pgtypes::ContractV1>())};
}
}  // namespace contracts::views::api::v1::contracts::get
