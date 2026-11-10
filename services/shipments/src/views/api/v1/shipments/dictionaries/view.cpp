#include <components/settings.hpp>
#include <utility>
#include <views/api/v1/shipments/dictionaries/view.hpp>

namespace shipments::views::api::v1::shipments::dictionaries {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.settings->Dictionaries(request.body, context);
    return Response200{.carriage_terms = std::move(result.carriage_terms),
        .incoterms = std::move(result.incoterms),
        .loading = std::move(result.loading),
        .route_types = std::move(result.route_types)};
}
}  // namespace shipments::views::api::v1::shipments::dictionaries
