#include <helpers/notifications.hpp>
#include <views/api/v1/notifications/list/view.hpp>

namespace notifications::views::api::v1::notifications::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::notifications::helpers;
    CheckOrganization(deps, context, request.body.organization_id);
    const auto& identity = platform::auth::GetIdentity(context);
    const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster,
        sql::kListNotifications, request.body.organization_id, identity.Issuer, identity.Subject,
        request.body.before, request.body.limit);
    using Row =
        std::tuple<std::string, std::string, std::string, std::int64_t, pg::TimePointTz, bool>;
    Response200 response;
    for (const auto& [id, shipment, number, revision, created, read] :
        rows.AsContainer<std::vector<Row>>(pg::kRowTag))
        response.items.push_back({.created_at = created.GetUnderlying(),
            .id = id,
            .read = read,
            .shipment_id = shipment,
            .shipment_number = number,
            .shipment_revision = revision});
    return response;
}
}  // namespace notifications::views::api::v1::notifications::list
