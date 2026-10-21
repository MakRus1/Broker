#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/counterparties/history/view.hpp>

namespace web_bff::views::api::v1::counterparties::history {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::counterparties::ApiV1CounterpartiesHistory;
    const auto result = deps.counterparties.ApiV1CounterpartiesHistory(
        api::Request{.body = Convert(request.body)}, session.Access);
    return Response200{.items = Convert(result.items)};
}
}  // namespace web_bff::views::api::v1::counterparties::history
