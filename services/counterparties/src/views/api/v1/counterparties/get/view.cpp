#include <helpers/details.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/counterparties/get/view.hpp>

namespace counterparties::views::api::v1::counterparties::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::counterparties::helpers;

    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::COUNTERPARTIES_READ);
    const auto organization = platform::auth::GetOrganization(identity);
    const auto rows = deps.pg_cluster->Execute(
        pg::ClusterHostType::kMaster, sql::kGetCounterparty, organization, request.body.id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Контрагент не найден"};
    return Response200{.counterparty = Convert(rows.AsSingleRow<pgtypes::CounterpartyV6>())};
}
}  // namespace counterparties::views::api::v1::counterparties::get
