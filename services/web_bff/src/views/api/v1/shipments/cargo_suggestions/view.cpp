#include <platform/browser/session.hpp>
#include <views/api/v1/shipments/cargo_suggestions/view.hpp>

namespace web_bff::views::api::v1::shipments::cargo_suggestions {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::shipments::ApiV1ShipmentsCargoSuggestions;
    const auto result = deps.shipments.ApiV1ShipmentsCargoSuggestions(
        api::Request{.body = {.query = request.body.query}}, session.Access);
    return Response200{.items = result.items};
}
}  // namespace web_bff::views::api::v1::shipments::cargo_suggestions
