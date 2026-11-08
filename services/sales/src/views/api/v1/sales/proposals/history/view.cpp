#include <helpers/database.hpp>
#include <views/api/v1/sales/proposals/history/view.hpp>

namespace sales::views::api::v1::sales::proposals::history {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::sales::helpers;
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kHistoryProposals,
        organization, request.body.id, request.body.after_revision.value_or(0));
    Response200 result;
    for (const auto& row : rows)
        result.items.push_back(models::ParseModelJson(
            row[0].As<json::Value>(), std::type_identity<models::ProposalHistoryEntry>{}));
    return result;
}
}  // namespace sales::views::api::v1::sales::proposals::history
