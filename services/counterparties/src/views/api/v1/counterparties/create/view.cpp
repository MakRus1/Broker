#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <platform/auth/principal.hpp>
#include <userver/utils/text_light.hpp>
#include <views/api/v1/counterparties/create/view.hpp>

namespace counterparties::views::api::v1::counterparties::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::counterparties::helpers;
    try {
        const auto& principal = platform::auth::GetPrincipal(context);
        const auto tenant = ResolveTenant(*deps.pg_cluster, principal);
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto name = userver::utils::text::Trim(request.body.name);
        if (name.empty())
            return Response400{.error = "empty_name", .message = "Укажите название контрагента"};
        const auto inserted =
            transaction.Execute(sql::kCreateCounterparty, tenant, request.body.id, name);
        // Чтение нужно только для повтора команды с уже существующим UUID.
        const auto row = inserted.IsEmpty()
            ? transaction.Execute(sql::kGetCounterparty, tenant, request.body.id)
                  .AsSingleRow<pgtypes::CounterpartyV1>()
            : inserted.AsSingleRow<pgtypes::CounterpartyV1>();
        if (row.Name != name)
            return Response409{
                .error = "id_conflict", .message = "Идентификатор занят другим контрагентом"};
        const auto result = Convert(row);
        if (!inserted.IsEmpty())
            WriteAudit(transaction, tenant, principal, "create_counterparty", result.id,
                request.body, result);
        transaction.Commit();
        return Response201{.counterparty = result};
    } catch (const ::counterparties::ApiError& error) {
        return Response403{.error = error.Code, .message = error.what()};
    }
}
}  // namespace counterparties::views::api::v1::counterparties::create
