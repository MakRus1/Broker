#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/counterparties/list/view.hpp>

namespace web_bff::views::api::v1::counterparties::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::counterparties::ApiV1CounterpartiesList;
    const auto result = deps.counterparties.ApiV1CounterpartiesList(
        api::Request{.body = Convert(request.body)}, session.Access);
    return Response200{.items = Convert(result.items), .next_cursor = Convert(result.next_cursor)};
}
}  // namespace web_bff::views::api::v1::counterparties::list
