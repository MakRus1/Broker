#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/counterparties/list/view.hpp>

namespace web_bff::views::api::v1::counterparties::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::counterparties::ApiV1CounterpartiesList;
    const auto body =
        api::ParseBody(userver::formats::json::ValueBuilder(request.body).ExtractValue());
    const auto result =
        deps.counterparties.ApiV1CounterpartiesList(api::Request{.body = body}, session.Access);
    Response200 response;
    for (const auto& item : result.items)
        response.items.push_back(Convert<models::Counterparty>(item));
    return response;
}
}  // namespace web_bff::views::api::v1::counterparties::list
