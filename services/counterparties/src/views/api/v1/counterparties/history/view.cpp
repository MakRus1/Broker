#include <helpers/database.hpp>
#include <helpers/history.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/counterparties/history/view.hpp>

namespace counterparties::views::api::v1::counterparties::history {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::counterparties::helpers;

    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::COUNTERPARTIES_READ);
    const auto organization = platform::auth::GetOrganization(identity);
    // Физического удаления карточек нет; история читается отдельным атомарным запросом.
    if (deps.pg_cluster
            ->Execute(
                pg::ClusterHostType::kMaster, sql::kGetCounterparty, organization, request.body.id)
            .IsEmpty())
        return Response404{.error = "not_found", .message = "Контрагент не найден"};
    const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kListHistory,
        organization, request.body.id, request.body.before, request.body.limit);
    return Response200{
        .items = Convert(rows.AsContainer<std::vector<pgtypes::CounterpartyEventV2>>())};
}
}  // namespace counterparties::views::api::v1::counterparties::history
