#include <localization/sql_queries.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <views/localization/catalog/view.hpp>

namespace localization::views::localization::catalog {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    const auto rows =
        deps.pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
            ::localization::sql::kGetPublicCatalog, EnumValue(request.locale));
    Response200 response;
    for (const auto& row : rows) {
        response.messages.push_back({.key = row[0].As<std::string>(),
            .value = row[1].As<userver::formats::json::Value>().As<std::string>()});
    }
    return response;
}
}  // namespace localization::views::localization::catalog
