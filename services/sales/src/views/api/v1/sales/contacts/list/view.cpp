#include <helpers/database.hpp>
#include <views/api/v1/sales/contacts/list/view.hpp>

namespace sales::views::api::v1::sales::contacts::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::sales::helpers;
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    if (deps.pg_cluster
            ->Execute(
                pg::ClusterHostType::kMaster, sql::kGetCards, organization, request.body.card_id)
            .IsEmpty())
        throw ApiError<404>{"not_found", "Карточка клиента не найдена в организации"};
    const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kListContacts,
        organization, request.body.card_id, request.body.before, request.body.limit + 1);
    Response200 response;
    for (const auto& row : rows.AsSetOf<json::Value>()) {
        if (response.items.size() == static_cast<std::size_t>(request.body.limit)) {
            response.next_before = response.items.back().id;
            break;
        }
        response.items.push_back(
            models::ParseModelJson(row, std::type_identity<models::SalesContact>{}));
    }
    return response;
}
}  // namespace sales::views::api::v1::sales::contacts::list
