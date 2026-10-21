#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/transportation_operations/create/view.hpp>

namespace web_bff::views::api::v1::transportation_operations::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::shipments::ApiV1TransportationOperationsCreate;
    const auto result = deps.shipments.ApiV1TransportationOperationsCreate(
        api::Request{.body = Convert(request.body)}, session.Access);
    return Response202{.operation = Convert(result.operation)};
}
}  // namespace web_bff::views::api::v1::transportation_operations::create
