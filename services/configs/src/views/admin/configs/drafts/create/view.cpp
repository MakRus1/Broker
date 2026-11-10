#include <components/config_store.hpp>
#include <converters/draft.hpp>
#include <platform/auth/identity.hpp>
#include <views/admin/configs/drafts/create/view.hpp>

namespace configs::views::admin::configs::drafts::create {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto draft = deps.store->CreateDraft(request.body, platform::auth::GetIdentity(context));
    return ::configs::converters::ToApiDraft<Response201>(draft);
}
}  // namespace configs::views::admin::configs::drafts::create
