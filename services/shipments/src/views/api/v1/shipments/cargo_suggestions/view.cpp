#include <helpers/shipment_card.hpp>
#include <views/api/v1/shipments/cargo_suggestions/view.hpp>

namespace shipments::views::api::v1::shipments::cargo_suggestions {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::shipments::helpers;
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    return Response200{
        .items =
            deps.pg_cluster
                ->Execute(pg::ClusterHostType::kSlave, sql::kListCargoSuggestions, organization,
                    userver::utils::text::ToLower(request.body.query.value_or("")))
                .AsContainer<std::vector<std::string>>()};
}
}  // namespace shipments::views::api::v1::shipments::cargo_suggestions
