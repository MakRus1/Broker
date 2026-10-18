#include <components/admin_api.hpp>
#include <views/admin/configs/drafts/edit/view.hpp>
namespace configs::views::admin::configs::drafts::edit {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    userver::formats::json::ValueBuilder body{userver::formats::json::Type::kObject};
    body["actor"] = request.body.actor;
    if (request.body.description)
        body["description"] = *request.body.description;
    body["expected_version"] = request.body.expected_version;
    body["id"] = request.body.id;
    if (request.body.value)
        body["value"] = *request.body.value;
    const ::configs::ConfigStore store{deps.pg_cluster};
    return ::configs::admin::Execute<Response, Response400, Response403, Response404, Response409>(
        [&] { return store.Draft("edit", body.ExtractValue()); },
        [](const platform::configs::Json& value) {
            return Response200{
                .approved_by = ::configs::admin::Optional<std::string>(value, "approved_by"),
                .author = value["author"].As<std::string>(),
                .base_value = value["base_value"],
                .base_version = value["base_version"].As<std::int64_t>(),
                .config_name = value["config_name"].As<std::string>(),
                .description = value["description"].As<std::string>(),
                .id = value["id"].As<std::int64_t>(),
                .published_revision =
                    ::configs::admin::Optional<std::int64_t>(value, "published_revision"),
                .source_revision =
                    ::configs::admin::Optional<std::int64_t>(value, "source_revision"),
                .status = ParseEnumJson(value["status"], status{}),
                .value = value["value"],
                .version = value["version"].As<std::int64_t>()};
        });
}
}  // namespace configs::views::admin::configs::drafts::edit
