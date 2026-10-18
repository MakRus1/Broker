#include <components/config_store.hpp>
#include <views/configs/values/view.hpp>

namespace configs::views::configs::values {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    const ::configs::ConfigStore store{deps.pg_cluster};
    const auto values = store.Values(request.body.ids);
    return Response200{
        .configs = values.Values, .revision = values.Revision, .updated_at = values.UpdatedAt};
}
}  // namespace configs::views::configs::values
