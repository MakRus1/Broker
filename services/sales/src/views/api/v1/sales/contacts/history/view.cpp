#include <helpers/settings.hpp>
#include <views/api/v1/sales/contacts/history/view.hpp>

namespace sales::views::api::v1::sales::contacts::history {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::sales::helpers;
    const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kHistoryContacts,
        platform::auth::GetOrganization(platform::auth::GetIdentity(context)), request.body.id,
        request.body.after_revision.value_or(0));
    Response200 response;
    response.items.reserve(rows.Size());
    for (const auto& row : rows.AsSetOf<json::Value>())
        response.items.push_back(
            models::ParseModelJson(row, std::type_identity<models::SalesContactHistoryEntry>{}));
    return response;
}
}  // namespace sales::views::api::v1::sales::contacts::history
