#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/transportations/get/view.hpp>

namespace web_bff::views::api::v1::transportations::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::transportations::ApiV1TransportationsGet;
    const auto body = Convert(request.body);
    const auto result =
        deps.transportations.ApiV1TransportationsGet(api::Request{.body = body}, session.Access);
    return Response200{.transportation = Convert(result.transportation)};
}
}  // namespace web_bff::views::api::v1::transportations::get
