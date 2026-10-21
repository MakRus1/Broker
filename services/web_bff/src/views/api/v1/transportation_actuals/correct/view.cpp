#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/transportation_actuals/correct/view.hpp>

namespace web_bff::views::api::v1::transportation_actuals::correct {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::transportations::ApiV1TransportationActualsCorrect;
    const auto result = deps.transportations.ApiV1TransportationActualsCorrect(
        api::Request{.body = Convert(request.body)}, session.Access);
    return Response200{.transportation = Convert(result.transportation)};
}
}  // namespace web_bff::views::api::v1::transportation_actuals::correct
