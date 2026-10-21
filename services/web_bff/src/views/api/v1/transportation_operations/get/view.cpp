#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/transportation_operations/get/view.hpp>

namespace web_bff::views::api::v1::transportation_operations::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::shipments::ApiV1TransportationOperationsGet;
    const auto result = deps.shipments.ApiV1TransportationOperationsGet(
        api::Request{.body = Convert(request.body)}, session.Access);
    return Response200{.operation = Convert(result.operation)};
}
}  // namespace web_bff::views::api::v1::transportation_operations::get
