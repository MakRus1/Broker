#include <components/config_store.hpp>
#include <views/admin/configs/current/view.hpp>

namespace configs::views::admin::configs::current {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    const auto config = deps.store->GetConfig(request.body.config_name);
    return Response200{.active_draft_id = config.ActiveDraftId,
        .allow_self_approval = deps.store->AllowSelfApproval(),
        .config_name = config.Name,
        .value = config.Value,
        .version = config.Version};
}
}  // namespace configs::views::admin::configs::current
