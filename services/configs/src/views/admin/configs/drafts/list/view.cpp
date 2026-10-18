#include <components/admin_api.hpp>
#include <views/admin/configs/drafts/list/view.hpp>
namespace configs::views::admin::configs::drafts::list {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    userver::formats::json::ValueBuilder body{userver::formats::json::Type::kObject};
    if (request.body.before_id)
        body["before_id"] = *request.body.before_id;
    if (request.body.config_name)
        body["config_name"] = *request.body.config_name;
    const ::configs::ConfigStore store{deps.pg_cluster};
    return ::configs::admin::Execute<Response, Response400, Response403, Response404, Response409>(
        [&] { return store.Draft("list", body.ExtractValue()); },
        [](const platform::configs::Json& value) {
            return Response200{.items = ::configs::admin::Items(value)};
        });
}
}  // namespace configs::views::admin::configs::drafts::list
