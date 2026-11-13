#include <components/editor.hpp>
#include <views/api/admin/localization/get/view.hpp>

namespace localization::views::api::admin::localization::get {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    const auto state = deps.editor->GetEditorState(request.body);
    return Response200{.draft = state.draft,
        .inherited = state.inherited,
        .published = state.published,
        .revision = state.revision,
        .source = state.source,
        .version = state.version};
}
}  // namespace localization::views::api::admin::localization::get
