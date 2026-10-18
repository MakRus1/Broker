#include <components/config_store.hpp>
#include <views/admin/configs/drafts/list/view.hpp>
#include <views/models.hpp>

namespace configs::views::admin::configs::drafts::list {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    const ::configs::ConfigStore store{deps.pg_cluster};
    try {
        const auto drafts = store.ListDrafts(
            {.ConfigName = request.body.config_name, .BeforeId = request.body.before_id});
        return Response200{.items = ::configs::views::ToApiDrafts(drafts)};
    } catch (const ::configs::InvalidRequest& error) {
        return Response400{.error = "invalid_request", .message = error.what()};
    }
}
}  // namespace configs::views::admin::configs::drafts::list
