#include <helpers/editor.hpp>
#include <views/api/admin/localization/publish/view.hpp>

namespace localization::views::api::admin::localization::publish {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    const auto scope = request.body.organization_id.value_or("global");
    const auto state =
        ::localization::helpers::ChangeEditor(deps, scope, request.body.version, std::nullopt);
    return Response200{.draft = state.draft,
        .inherited = state.inherited,
        .published = state.published,
        .revision = state.revision,
        .source = state.source,
        .version = state.version};
}
}  // namespace localization::views::api::admin::localization::publish
