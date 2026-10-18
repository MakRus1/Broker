#include <components/config_store.hpp>
#include <views/admin/configs/history/view.hpp>
#include <views/serialize.hpp>

namespace configs::views::admin::configs::history {
Response View::Handle(Request&&, userver::server::request::RequestContext&, Deps deps) {
    const ::configs::ConfigStore store{deps.pg_cluster};
    return Response200{.items = ::configs::views::SerializeHistory(store.History())};
}
}  // namespace configs::views::admin::configs::history
