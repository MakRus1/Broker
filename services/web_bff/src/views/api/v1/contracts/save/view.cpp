#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/contracts/save/view.hpp>

namespace web_bff::views::api::v1::contracts::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::contracts::ApiV1ContractsSave;
    const auto result = deps.contracts.ApiV1ContractsSave(
        api::Request{.body = Convert(request.body)}, session.Access);
    return Response200{.contract = Convert(result.contract)};
}
}  // namespace web_bff::views::api::v1::contracts::save
