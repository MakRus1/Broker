#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <platform/auth/principal.hpp>
#include <views/api/v1/counterparties/list/view.hpp>

namespace counterparties::views::api::v1::counterparties::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::counterparties::helpers;
    try {
        const auto& principal = platform::auth::GetPrincipal(context);
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto tenant = ResolveTenant(transaction, principal);
        const auto rows = transaction.Execute(
            sql::kListCounterparties, tenant, request.body.after, request.body.limit);
        std::vector<models::Counterparty> items;
        for (const auto& row : rows.AsSetOf<pgtypes::CounterpartyV1>())
            items.push_back(Convert(row));
        transaction.Commit();
        return Response200{.items = items};
    } catch (const ::counterparties::ApiError& error) {
        return Response403{.error = error.Code, .message = error.what()};
    }
}
}  // namespace counterparties::views::api::v1::counterparties::list
