#include <helpers/settings.hpp>
#include <views/api/v1/sales/cards/get/view.hpp>

namespace sales::views::api::v1::sales::cards::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::sales::helpers;
    const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kGetCards,
        platform::auth::GetOrganization(platform::auth::GetIdentity(context)), request.body.id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Запись не найдена в организации"};
    return Response200{.item = models::ParseModelJson(rows.AsSingleRow<json::Value>(),
                           std::type_identity<models::SalesCard>{})};
}
}  // namespace sales::views::api::v1::sales::cards::get
