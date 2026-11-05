#include <boost/uuid/string_generator.hpp>
#include <helpers/notifications.hpp>
#include <views/api/v1/notifications/read/view.hpp>

namespace notifications::views::api::v1::notifications::read {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::notifications::helpers;
    CheckOrganization(deps, context, request.body.organization_id);
    const auto& identity = platform::auth::GetIdentity(context);
    std::vector<boost::uuids::uuid> ids;
    for (const auto& id : request.body.ids)
        ids.push_back(boost::uuids::string_generator{}(id));
    const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster,
        sql::kReadNotifications, request.body.organization_id, identity.Issuer, identity.Subject,
        ids, request.body.dismiss);
    return Response200{.updated = static_cast<int>(rows.Size())};
}
}  // namespace notifications::views::api::v1::notifications::read
