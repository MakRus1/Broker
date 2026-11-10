#include <components/config_store.hpp>
#include <converters/revision.hpp>
#include <views/admin/configs/history/view.hpp>

namespace configs::views::admin::configs::history {
Response View::Handle(Request&&, userver::server::request::RequestContext&, Deps deps) {
    return Response200{.items = ::configs::converters::ToApiHistory(deps.store->History())};
}
}  // namespace configs::views::admin::configs::history
