#include <components/config_store.hpp>
#include <views/admin/configs/current/view.hpp>

namespace configs::views::admin::configs::current {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    const ::configs::ConfigStore store{deps.pg_cluster};
    try {
        const auto config = store.GetConfig(request.body.config_name);
        return Response200{.active_draft_id = config.ActiveDraftId,
            .allow_self_approval = deps.allow_self_approval,
            .config_name = config.Name,
            .value = config.Value,
            .version = config.Version};
    } catch (const ::configs::InvalidRequest& error) {
        return Response400{.error = "invalid_request", .message = error.what()};
    }
}
}  // namespace configs::views::admin::configs::current
