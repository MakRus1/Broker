#include <helpers/converters.hpp>
#include <helpers/registry_filters.hpp>
#include <platform/browser/session.hpp>
#include <views/api/v1/service_charges/filter_values/view.hpp>

namespace web_bff::views::api::v1::service_charges::filter_values {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    namespace api = clients::finance::ApiV1ServiceChargesFilterValues;
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto response = deps.finance.ApiV1ServiceChargesFilterValues(
        api::Request{.body = {.after = request.body.after,
                         .candidates = request.body.candidates,
                         .field = request.body.field,
                         .limit = request.body.limit,
                         .query = request.body.query,
                         .registry = ConvertRegistry(request.body.registry)}},
        session.Access);
    Response200 result;
    result.next_after = response.next_after;
    for (const auto& field : response.fields)
        result.fields.push_back(ConvertRegistryField<models::finance::RegistryField>(field));
    for (const auto& value : response.values)
        result.values.push_back({.label = value.label, .value = value.value});
    return result;
}
}  // namespace web_bff::views::api::v1::service_charges::filter_values
