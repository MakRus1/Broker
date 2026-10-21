#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <platform/auth/principal.hpp>
#include <views/api/v1/counterparties/create/view.hpp>

namespace counterparties::views::api::v1::counterparties::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::counterparties::helpers;
    try {
        const auto& principal = platform::auth::GetPrincipal(context);
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto tenant = ResolveTenant(transaction, principal);
        const auto first = request.body.name.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
            return Response400{.error = "empty_name", .message = "Укажите название контрагента"};
        const auto last = request.body.name.find_last_not_of(" \t\r\n");
        const auto name = request.body.name.substr(first, last - first + 1);
        const auto inserted =
            transaction.Execute(sql::kCreateCounterparty, tenant, request.body.id, name);
        const auto row = transaction.Execute(sql::kGetCounterparty, tenant, request.body.id)
                             .AsSingleRow<pgtypes::CounterpartyV1>();
        if (row.Name != name)
            return Response409{
                .error = "id_conflict", .message = "Идентификатор занят другим контрагентом"};
        const auto result = Convert(row);
        if (inserted.RowsAffected() != 0)
            WriteAudit(transaction, tenant, principal, "create_counterparty", result.id,
                request.body, result);
        transaction.Commit();
        return Response201{.counterparty = result};
    } catch (const ::counterparties::ApiError& error) {
        return Response403{.error = error.Code, .message = error.what()};
    }
}
}  // namespace counterparties::views::api::v1::counterparties::create
