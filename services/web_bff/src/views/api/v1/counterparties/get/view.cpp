#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/counterparties/get/view.hpp>

namespace web_bff::views::api::v1::counterparties::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::counterparties::ApiV1CounterpartiesGet;
    const auto result = deps.counterparties.ApiV1CounterpartiesGet(
        api::Request{.body = Convert(request.body)}, session.Access);
    return Response200{.counterparty = Convert(result.counterparty)};
}
}  // namespace web_bff::views::api::v1::counterparties::get
