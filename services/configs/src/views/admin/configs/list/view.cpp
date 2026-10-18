#include <components/config_store.hpp>
#include <views/admin/configs/list/view.hpp>
#include <views/models.hpp>

namespace configs::views::admin::configs::list {
Response View::Handle(Request&&, userver::server::request::RequestContext&, Deps deps) {
    const ::configs::ConfigStore store{deps.pg_cluster};
    return Response200{.items = ::configs::views::ToApiCatalog(store.Catalog())};
}
}  // namespace configs::views::admin::configs::list
