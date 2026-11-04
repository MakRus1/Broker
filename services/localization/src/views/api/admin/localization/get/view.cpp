#include <helpers/editor.hpp>
#include <views/api/admin/localization/get/view.hpp>

namespace localization::views::api::admin::localization::get {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    const auto scope = request.body.organization_id.value_or("global");
    const auto state = ::localization::helpers::GetEditor(deps, scope);
    return Response200{.draft = state.draft,
        .inherited = state.inherited,
        .published = state.published,
        .revision = state.revision,
        .version = state.version};
}
}  // namespace localization::views::api::admin::localization::get
