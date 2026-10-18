#include <components/admin_api.hpp>
#include <views/admin/configs/current/view.hpp>
namespace configs::views::admin::configs::current {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    userver::formats::json::ValueBuilder body{userver::formats::json::Type::kObject};
    body["config_name"] = request.body.config_name;
    const ::configs::ConfigStore store{deps.pg_cluster};
    return ::configs::admin::Execute<Response, Response400, Response403, Response404, Response409>(
        [&] { return store.GetConfig(request.body.config_name); },
        [](const platform::configs::Json& value) {
            return Response200{.active_draft_id = ::configs::admin::Optional<std::int64_t>(
                                   value, "active_draft_id"),
                               .config_name = value["config_name"].As<std::string>(),
                               .value = value["value"],
                               .version = value["version"].As<std::int64_t>()};
        });
}
}  // namespace configs::views::admin::configs::current
