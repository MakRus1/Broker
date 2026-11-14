#include <components/config_store.hpp>
#include <views/configs/values/view.hpp>

namespace configs::views::configs::values {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    auto values = deps.store->Values(request.body.ids);
    return Response200{.configs = std::move(values.Values),
        .kill_switches_disabled = std::move(values.BuiltinKeys),
        .revision = values.Revision,
        .updated_at = values.PublishedAt};
}
}  // namespace configs::views::configs::values
