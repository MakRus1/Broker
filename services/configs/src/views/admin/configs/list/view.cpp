#include <components/admin_api.hpp>
#include <views/admin/configs/list/view.hpp>
namespace configs::views::admin::configs::list {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {

    const ::configs::ConfigStore store{deps.pg_cluster};
    return ::configs::admin::Execute<Response, Response400, Response403, Response404, Response409>(
        [&] { return store.Catalog(); },
        [](const platform::configs::Json& value) {
            return Response200{.items = ::configs::admin::Items(value)};
        });
}
}  // namespace configs::views::admin::configs::list
