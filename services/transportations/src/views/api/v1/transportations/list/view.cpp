#include <helpers/transportations.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/transportations/list/view.hpp>

namespace transportations::views::api::v1::transportations::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::transportations::helpers;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        platform::auth::CheckPermission(context, permissions::TRANSPORTATIONS_READ);
        // Перевозки и плечи страницы должны относиться к одному снимку.
        auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, kReadSnapshot);
        const auto organization = platform::auth::GetOrganization(identity);
        const auto rows =
            transaction
                .Execute(sql::kListTransportations, organization, request.body.after,
                    request.body.shipment_id, request.body.item_id, request.body.limit)
                .AsContainer<std::vector<pgtypes::TransportationV2>>();
        std::vector<std::string> ids;
        ids.reserve(rows.size());
        for (const auto& row : rows)
            ids.push_back(boost::uuids::to_string(row.Id));
        // Все плечи страницы читаются одним запросом, без N+1.
        const auto route_segments = ids.empty()
            ? std::vector<pgtypes::RouteSegmentV1>{}
            : transaction.Execute(sql::kListRouteSegments, organization, ids)
                  .AsContainer<std::vector<pgtypes::RouteSegmentV1>>();
        const auto result = Convert(rows, route_segments);
        transaction.Commit();
        return Response200{.items = result};
    } catch (const ::transportations::ApiError& error) {
        switch (error.Status) {
        case 400:
            return Response400{.error = error.Code, .message = error.what()};
        case 403:
            return Response403{.error = error.Code, .message = error.what()};
        case 404:
            return Response404{.error = error.Code, .message = error.what()};
        default:
            return Response409{.error = error.Code, .message = error.what()};
        }
    }
}
}  // namespace transportations::views::api::v1::transportations::list
