#include <components/config_store.hpp>
#include <converters/draft.hpp>
#include <views/admin/configs/drafts/list/view.hpp>

namespace configs::views::admin::configs::drafts::list {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    const auto drafts = deps.store->ListDrafts(request.body);
    return Response200{.items = ::configs::converters::ToApiDrafts(drafts)};
}
}  // namespace configs::views::admin::configs::drafts::list
