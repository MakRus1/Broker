#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/transportations/history/view.hpp>

namespace web_bff::views::api::v1::transportations::history {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::transportations::ApiV1TransportationsHistory;
    const auto result = deps.transportations.ApiV1TransportationsHistory(
        api::Request{.body = Convert(request.body)}, session.Access);
    return Response200{.items = Convert(result.items)};
}
}  // namespace web_bff::views::api::v1::transportations::history
