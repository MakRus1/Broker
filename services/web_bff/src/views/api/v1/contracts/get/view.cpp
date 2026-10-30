#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/contracts/get/view.hpp>

namespace web_bff::views::api::v1::contracts::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::contracts::ApiV1ContractsGet;
    const auto result = deps.contracts.ApiV1ContractsGet(
        api::Request{.body = {.id = request.body.id}}, session.Access);
    return Response200{.contract = Convert(result.contract)};
}
}  // namespace web_bff::views::api::v1::contracts::get
