#include <components/shipments.hpp>
#include <utility>
#include <views/api/v1/shipments/transportation_contexts/view.hpp>

namespace shipments::views::api::v1::shipments::transportation_contexts {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    auto result = deps.shipments->GetTransportationContexts(request.body, context);
    return Response200{.items = std::move(result.items)};
}
}  // namespace shipments::views::api::v1::shipments::transportation_contexts
