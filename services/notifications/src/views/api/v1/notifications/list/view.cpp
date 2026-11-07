#include <helpers/notifications.hpp>
#include <views/api/v1/notifications/list/view.hpp>

namespace notifications::views::api::v1::notifications::list {
namespace {
struct NotificationRow {
    std::string Id;
    std::string ShipmentId;
    std::string ShipmentNumber;
    std::int64_t ShipmentRevision;
    userver::storages::postgres::TimePointTz CreatedAt;
    bool Read;
    std::string EventType;
};
}  // namespace

Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::notifications::helpers;
    CheckOrganization(deps, context, request.body.organization_id);
    const auto& identity = platform::auth::GetIdentity(context);
    const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster,
        sql::kListNotifications, request.body.organization_id, identity.Issuer, identity.Subject,
        request.body.before, request.body.limit);
    Response200 response;
    response.items.reserve(rows.Size());
    for (const auto& row : rows.AsSetOf<NotificationRow>(pg::kRowTag))
        response.items.push_back({.created_at = row.CreatedAt.GetUnderlying(),
            .event_type = models::ParseEnumScalar(row.EventType, models::Notification_event_type{}),
            .id = row.Id,
            .read = row.Read,
            .shipment_id = row.ShipmentId,
            .shipment_number = row.ShipmentNumber,
            .shipment_revision = row.ShipmentRevision});
    return response;
}
}  // namespace notifications::views::api::v1::notifications::list
