#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/service_charges/save/view.hpp>

namespace web_bff::views::api::v1::service_charges::save {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::finance::ApiV1ServiceChargesSave;
    const auto result = deps.finance.ApiV1ServiceChargesSave(
        api::Request{.body = Convert(request.body)}, session.Access);
    return Response200{.charge = Convert(result.charge)};
}
}  // namespace web_bff::views::api::v1::service_charges::save
