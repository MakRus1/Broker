#include <helpers/shipment_card.hpp>
#include <views/api/v1/shipments/cargo_suggestions/remove/view.hpp>

namespace shipments::views::api::v1::shipments::cargo_suggestions::remove {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster,
        sql::kRemoveCargoSuggestion, organization, request.body.value);
    return Response200{.deleted = !rows.IsEmpty()};
}
}  // namespace shipments::views::api::v1::shipments::cargo_suggestions::remove
