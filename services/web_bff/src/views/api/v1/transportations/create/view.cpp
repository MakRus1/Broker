#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/transportations/create/view.hpp>

namespace web_bff::views::api::v1::transportations::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::transportations::ApiV1TransportationsCreate;
    const auto result = deps.transportations.ApiV1TransportationsCreate(
        api::Request{.body = Convert(request.body)}, session.Access);
    return Response200{.transportation = Convert(result.transportation)};
}
}  // namespace web_bff::views::api::v1::transportations::create
