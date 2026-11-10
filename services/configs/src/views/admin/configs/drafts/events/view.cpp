#include <components/config_store.hpp>
#include <converters/event.hpp>
#include <views/admin/configs/drafts/events/view.hpp>

namespace configs::views::admin::configs::drafts::events {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    const auto events = deps.store->DraftEvents(request.body.id);
    return Response200{.items = ::configs::converters::ToApiDraftEvents(events)};
}
}  // namespace configs::views::admin::configs::drafts::events
