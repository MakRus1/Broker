#include <platform/browser/session.hpp>
#include <views/api/v1/shipments/cargo_suggestions/remove/view.hpp>

namespace web_bff::views::api::v1::shipments::cargo_suggestions::remove {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::shipments::ApiV1ShipmentsCargoSuggestionsRemove;
    const auto result = deps.shipments.ApiV1ShipmentsCargoSuggestionsRemove(
        api::Request{
            .body = {.customer_id = request.body.customer_id, .value = request.body.value}},
        session.Access);
    return Response200{.deleted = result.deleted};
}
}  // namespace web_bff::views::api::v1::shipments::cargo_suggestions::remove
