#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/transportations/history/view.hpp>

namespace transportations::views::api::v1::transportations::history {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::transportations::helpers;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::TRANSPORTATIONS_READ);
        // Наличие перевозки и её история проверяются в одном снимке.
        auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, kReadSnapshot);
        const auto organization = platform::auth::GetOrganization(identity);
        if (transaction
                .Execute(sql::kGetTransportation, organization, request.body.transportation_id)
                .IsEmpty())
            return Response404{.error = "not_found", .message = "Перевозка не найдена"};
        const auto rows = transaction.Execute(sql::kListHistory, organization,
            request.body.transportation_id, request.body.before, request.body.limit);
        transaction.Commit();
        return Response200{
            .items = Convert(rows.AsContainer<std::vector<pgtypes::TransportationEventV2>>())};
    } catch (const ::transportations::ApiError& error) {
        return Response403{.error = error.Code, .message = error.what()};
    }
}
}  // namespace transportations::views::api::v1::transportations::history
