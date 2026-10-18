#include <components/admin_api.hpp>
#include <views/configs/values/view.hpp>
namespace configs::views::configs::values {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    userver::formats::json::ValueBuilder body{userver::formats::json::Type::kObject};
    if (request.body.ids)
        body["ids"] = *request.body.ids;
    if (request.body.updated_since)
        body["updated_since"] = *request.body.updated_since;
    const ::configs::ConfigStore store{deps.pg_cluster};
    return ::configs::admin::Execute<Response, Response400, Response403, Response404, Response409>(
        [&] { return store.Values(request.body.ids); },
        [](const platform::configs::Json& value) {
            return Response200{.configs = value["configs"],
                               .revision = value["revision"].As<std::int64_t>(),
                               .updated_at = value["updated_at"].As<std::string>()};
        });
}
}  // namespace configs::views::configs::values
