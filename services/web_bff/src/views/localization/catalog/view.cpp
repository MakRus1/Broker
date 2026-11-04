#include <views/localization/catalog/view.hpp>

namespace web_bff::views::localization::catalog {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    namespace api = clients::localization::LocalizationCatalog;
    const auto result = deps.localization.LocalizationCatalog(
        api::Request{.locale = api::ParseEnumScalar(EnumValue(request.locale), api::locale{})});
    Response200 response;
    for (const auto& message : result.messages)
        response.messages.push_back({.key = message.key, .value = message.value});
    return response;
}
}  // namespace web_bff::views::localization::catalog
