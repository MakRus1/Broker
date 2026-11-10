#include <components/config_store.hpp>
#include <views/configs/values/view.hpp>

namespace configs::views::configs::values {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    const auto values = deps.store->Values(request.body.ids);
    return Response200{
        .configs = values.Values, .revision = values.Revision, .updated_at = values.PublishedAt};
}
}  // namespace configs::views::configs::values
