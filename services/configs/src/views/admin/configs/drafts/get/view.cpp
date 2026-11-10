#include <components/config_store.hpp>
#include <converters/draft.hpp>
#include <views/admin/configs/drafts/get/view.hpp>

namespace configs::views::admin::configs::drafts::get {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    const auto draft = deps.store->GetDraft(request.body.id);
    return ::configs::converters::ToApiDraft<Response200>(draft);
}
}  // namespace configs::views::admin::configs::drafts::get
