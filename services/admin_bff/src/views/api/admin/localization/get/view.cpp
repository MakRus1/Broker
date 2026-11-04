#include <platform/browser/session.hpp>
#include <userver/formats/json.hpp>
#include <views/api/admin/localization/get/view.hpp>

namespace admin_bff::views::api::admin::localization::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    namespace api = clients::localization::ApiAdminLocalizationGet;
    const auto& session = platform::browser::GetBrowserSession(context);
    const api::RequestBody body{.organization_id = request.body.organization_id};
    const auto result =
        deps.localization.ApiAdminLocalizationGet(api::Request{.body = body}, session.Access);
    return Response200{.draft = result.draft
            ? std::optional{broker_codegen::ParseJson<models::EditableCatalog>(
                  userver::formats::json::ValueBuilder(*result.draft).ExtractValue())}
            : std::nullopt,
        .inherited = broker_codegen::ParseJson<models::EditableCatalog>(
            userver::formats::json::ValueBuilder(result.inherited).ExtractValue()),
        .published = broker_codegen::ParseJson<models::EditableCatalog>(
            userver::formats::json::ValueBuilder(result.published).ExtractValue()),
        .revision = result.revision,
        .version = result.version};
}
}  // namespace admin_bff::views::api::admin::localization::get
