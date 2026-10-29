#include <helpers/database.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <userver/formats/json.hpp>
#include <views/api/internal/v1/shipments/route_projection/view.hpp>

namespace transportations::views::api::internal::v1::shipments::route_projection {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::transportations::helpers;
    platform::auth::CheckPermission(context, permissions::TRANSPORTATIONS_CREATE);
    const auto& body = request.body;
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto advanced = transaction.Execute(
        sql::kAdvanceRouteProjection, body.organization_id, body.shipment_id, body.revision);
    if (advanced.IsEmpty()) {
        transaction.Commit();
        return Response200{.applied = false};
    }
    transaction.Execute(sql::kClearRouteProjection, body.organization_id, body.shipment_id);
    for (const auto& segment : body.route_segments)
        transaction.Execute(sql::kSaveRouteProjectionSegment, body.organization_id,
            body.shipment_id, segment.id, segment.type,
            userver::formats::json::ValueBuilder(segment).ExtractValue());
    transaction.Commit();
    return Response200{.applied = true};
}
}  // namespace transportations::views::api::internal::v1::shipments::route_projection
