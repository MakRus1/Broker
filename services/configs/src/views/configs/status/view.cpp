#include <components/config_store.hpp>
#include <views/configs/status/view.hpp>

namespace configs::views::configs::status {
Response View::Handle(Request&&, userver::server::request::RequestContext&, Deps deps) {
    const ::configs::ConfigStore store{deps.pg_cluster};
    return Response200{.revision = store.Current().Id};
}
}  // namespace configs::views::configs::status
