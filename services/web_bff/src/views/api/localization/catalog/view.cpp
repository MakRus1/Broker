#include <platform/browser/session.hpp>
#include <views/api/localization/catalog/view.hpp>

namespace web_bff::views::api::localization::catalog {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    namespace api = clients::localization::ApiV1LocalizationCatalog;
    namespace models = clients::localization::models;
    const auto result = deps.localization.ApiV1LocalizationCatalog(
        api::Request{.body = {.locale = models::ParseEnumScalar(EnumValue(request.body.locale),
                                  models::GetCatalogRequest_locale{}),
                         .organization_id = request.body.organization_id}},
        session.Access);
    Response200 response;
    for (const auto& message : result.messages)
        response.messages.push_back({.key = message.key, .value = message.value});
    return response;
}
}  // namespace web_bff::views::api::localization::catalog
