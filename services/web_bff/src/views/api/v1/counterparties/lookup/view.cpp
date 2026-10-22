#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/counterparties/lookup/view.hpp>

namespace web_bff::views::api::v1::counterparties::lookup {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::counterparties::ApiV1CounterpartiesLookup;
    const auto result = deps.counterparties.ApiV1CounterpartiesLookup(
        api::Request{.body = {.tax_id = request.body.tax_id}}, session.Access);
    return Response200{.items = Convert(result.items)};
}
}  // namespace web_bff::views::api::v1::counterparties::lookup
