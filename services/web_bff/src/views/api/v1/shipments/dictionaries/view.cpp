#include <platform/browser/session.hpp>
#include <views/api/v1/shipments/dictionaries/view.hpp>

namespace web_bff::views::api::v1::shipments::dictionaries {
Response View::Handle(Request&&, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::shipments::ApiV1ShipmentsDictionaries;
    const auto source = deps.shipments.ApiV1ShipmentsDictionaries(api::Request{}, session.Access);
    const auto convert = [](const auto& values) {
        std::vector<models::shipments::DictionaryValue> result;
        result.reserve(values.size());
        for (const auto& value : values)
            result.push_back({.code = value.code, .name = value.name});
        return result;
    };
    return Response200{.carriage_terms = convert(source.carriage_terms),
        .incoterms = convert(source.incoterms),
        .loading = convert(source.loading),
        .route_types = convert(source.route_types)};
}
}  // namespace web_bff::views::api::v1::shipments::dictionaries
