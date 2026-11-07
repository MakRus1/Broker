#include <helpers/settings.hpp>
#include <views/api/v1/sales/cards/get/view.hpp>

namespace sales::views::api::v1::sales::cards::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::sales::helpers;
    return Response200{
        .item = Read<models::SalesCard>(deps.pg_cluster->Execute(
            pg::ClusterHostType::kMaster, sql::kGetCards, Organization(context), request.body.id))};
}
}  // namespace sales::views::api::v1::sales::cards::get
