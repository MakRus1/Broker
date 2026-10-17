#include <components/config_store.hpp>
#include <configs/sql_queries.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <views/configs/status/view.hpp>

namespace configs::views::configs::status {
Response View::Handle(Request&&, userver::server::request::RequestContext&, Deps deps) {
    const auto rows = deps.pg_cluster->Execute(
        userver::storages::postgres::ClusterHostType::kMaster, ::configs::sql::kLastRevision);
    return Response200{
        .revision =
            rows.IsEmpty()
                ? 0
                : rows.AsSingleRow<::configs::Revision>(userver::storages::postgres::kRowTag).Id};
}
}  // namespace configs::views::configs::status
