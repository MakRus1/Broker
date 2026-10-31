#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/service_charges/registry/view.hpp>

namespace web_bff::views::api::v1::service_charges::registry {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result =
        deps.finance.ApiV1ServiceChargesRegistry({.body = Convert(request.body)}, session.Access);
    return Response200{.invoices = Convert(result.invoices),
        .items = Convert(result.items),
        .next_cursor = Convert(result.next_cursor)};
}
}  // namespace web_bff::views::api::v1::service_charges::registry
