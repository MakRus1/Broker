#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <platform/auth/principal.hpp>
#include <views/api/v1/transportations/history/view.hpp>

namespace transportations::views::api::v1::transportations::history {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::transportations::helpers;
    try {
        const auto& principal = platform::auth::GetPrincipal(context);
        // Наличие перевозки и её история проверяются в одном снимке.
        auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, kReadSnapshot);
        const auto tenant = ResolveTenant(transaction, principal);
        if (transaction.Execute(sql::kGetTransportation, tenant, request.body.transportation_id)
                .IsEmpty())
            return Response404{.error = "not_found", .message = "Перевозка не найдена"};
        const auto rows = transaction.Execute(sql::kListHistory, tenant,
            request.body.transportation_id, request.body.before, request.body.limit);
        transaction.Commit();
        return Response200{
            .items = Convert(rows.AsContainer<std::vector<pgtypes::TransportationEventV1>>())};
    } catch (const ::transportations::ApiError& error) {
        return Response403{.error = error.Code, .message = error.what()};
    }
}
}  // namespace transportations::views::api::v1::transportations::history
