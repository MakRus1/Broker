#include <components/config_store.hpp>
#include <converters/draft.hpp>
#include <platform/auth/identity.hpp>
#include <views/admin/configs/drafts/edit/view.hpp>

namespace configs::views::admin::configs::drafts::edit {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto draft = deps.store->EditDraft(request.body, platform::auth::GetIdentity(context));
    return ::configs::converters::ToApiDraft<Response200>(draft);
}
}  // namespace configs::views::admin::configs::drafts::edit
