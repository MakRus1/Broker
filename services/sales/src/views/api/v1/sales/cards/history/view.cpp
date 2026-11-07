#include <helpers/settings.hpp>
#include <views/api/v1/sales/cards/history/view.hpp>

namespace sales::views::api::v1::sales::cards::history {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::sales::helpers;
    return Response200{.items = ReadList<models::SalesHistoryEntry>(deps.pg_cluster->Execute(
                           pg::ClusterHostType::kMaster, sql::kHistoryCards, Organization(context),
                           request.body.id, request.body.after_revision.value_or(0)))};
}
}  // namespace sales::views::api::v1::sales::cards::history
