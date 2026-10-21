#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/counterparties/create/view.hpp>

namespace web_bff::views::api::v1::counterparties::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::counterparties::ApiV1CounterpartiesCreate;
    const auto result = deps.counterparties.ApiV1CounterpartiesCreate(
        api::Request{.body = Convert(request.body)}, session.Access);
    return Response201{.counterparty = Convert(result.counterparty)};
}
}  // namespace web_bff::views::api::v1::counterparties::create
