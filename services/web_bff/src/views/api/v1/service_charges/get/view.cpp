#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/service_charges/get/view.hpp>

namespace web_bff::views::api::v1::service_charges::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result =
        deps.finance.ApiV1ServiceChargesGet({.body = {.id = request.body.id}}, session.Access);
    return Response200{.charge = Convert(result.charge)};
}
}  // namespace web_bff::views::api::v1::service_charges::get
