#include <helpers/actuals.hpp>
#include <helpers/inherited_route.hpp>
#include <helpers/revision.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/transportation_actuals/record/view.hpp>

namespace transportations::views::api::v1::transportation_actuals::record {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::transportations::helpers;
    try {
        platform::auth::CheckPermission(context, permissions::TRANSPORTATIONS_WRITE);
        const auto& identity = platform::auth::GetIdentity(context);
        auto loaded = LoadLocalTransportation(deps, context, request.body.transportation_id);
        CheckRevision(loaded.revision, request.body.expected_revision);
        const auto sources = LoadShipmentContexts(deps, context, {loaded.shipment_id});
        const auto& source = FindShipmentContext(sources, loaded.shipment_id);
        InheritRoute(loaded, source);
        ApplyActual(loaded, request.body.route_segment_id, request.body.event_kind,
            request.body.occurred_at, ActualChange::Record);
        RegisterRouteUsage(deps, context, source, {request.body.route_segment_id});
        auto transaction =
            deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
        const auto organization = platform::auth::GetOrganization(identity);
        auto result =
            ReadTransportation(transaction, organization, request.body.transportation_id, true);
        CheckRevision(result.revision, request.body.expected_revision);
        InheritRoute(result, source);
        UpdateActual(transaction, organization, result, request.body.route_segment_id,
            request.body.event_kind, request.body.occurred_at, ActualChange::Record);
        WriteAudit(
            transaction, organization, identity, "record_actual", result.id, request.body, result);
        transaction.Commit();
        return Response200{.transportation = result};
    } catch (const ::transportations::ApiError& error) {
        switch (error.Status) {
        case 400:
            return Response400{.error = error.Code, .message = error.what()};
        case 403:
            return Response403{.error = error.Code, .message = error.what()};
        case 404:
            return Response404{.error = error.Code, .message = error.what()};
        case 503:
            return Response503{.error = error.Code, .message = error.what()};
        default:
            return Response409{.error = error.Code, .message = error.what()};
        }
    }
}
}  // namespace transportations::views::api::v1::transportation_actuals::record
