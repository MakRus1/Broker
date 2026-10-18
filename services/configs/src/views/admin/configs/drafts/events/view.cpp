#include <components/admin_api.hpp>
#include <views/admin/configs/drafts/events/view.hpp>
namespace configs::views::admin::configs::drafts::events {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    userver::formats::json::ValueBuilder body{userver::formats::json::Type::kObject};
    body["id"] = request.body.id;
    const ::configs::ConfigStore store{deps.pg_cluster};
    return ::configs::admin::Execute<Response, Response400, Response403, Response404, Response409>(
        [&] { return store.DraftEvents(request.body.id); },
        [](const platform::configs::Json& value) {
            return Response200{.items = ::configs::admin::Items(value)};
        });
}
}  // namespace configs::views::admin::configs::drafts::events
