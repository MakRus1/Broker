#include <components/config_store.hpp>
#include <views/admin/configs/drafts/events/view.hpp>
#include <views/models.hpp>

namespace configs::views::admin::configs::drafts::events {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    const ::configs::ConfigStore store{deps.pg_cluster};
    try {
        const auto events = store.DraftEvents(request.body.id);
        return Response200{.items = ::configs::views::ToApiDraftEvents(events)};
    } catch (const ::configs::InvalidRequest& error) {
        return Response400{.error = "invalid_request", .message = error.what()};
    } catch (const ::configs::RevisionNotFound& error) {
        return Response404{.error = "not_found", .message = error.what()};
    }
}
}  // namespace configs::views::admin::configs::drafts::events
