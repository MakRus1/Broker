#include <components/config_store.hpp>
#include <converters/config.hpp>
#include <views/admin/configs/list/view.hpp>

namespace configs::views::admin::configs::list {
Response View::Handle(Request&&, userver::server::request::RequestContext&, Deps deps) {
    return Response200{.items = ::configs::converters::ToApiCatalog(deps.store->Catalog())};
}
}  // namespace configs::views::admin::configs::list
