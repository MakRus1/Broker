#include <helpers/converters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/payments/filter_values/view.hpp>

namespace web_bff::views::api::v1::payments::filter_values {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto result =
        deps.finance.ApiV1PaymentsFilterValues({.body = Convert(request.body)}, session.Access);
    Response200 response;
    response.next_after = result.next_after;
    for (const auto& field : result.fields)
        response.fields.push_back(ConvertRegistryField<models::finance::RegistryField>(field));
    for (const auto& value : result.values)
        response.values.push_back({.label = value.label, .value = value.value});
    return response;
}
}  // namespace web_bff::views::api::v1::payments::filter_values
