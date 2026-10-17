#include <views/hello_postgres/create/view.hpp>

#include <broker/sql_queries.hpp>
#include <domain/greeting.hpp>

#include <userver/storages/postgres/cluster.hpp>

namespace broker::views::hello_postgres::create {

Response View::Handle(Request&& request, userver::server::request::RequestContext& /*context*/,
                      Deps deps) {
    const auto name = request.name.value_or("");
    auto user_type = domain::UserType::kFirstTime;

    if (!name.empty()) {
        const auto result = deps.pg_cluster->Execute(
            userver::storages::postgres::ClusterHostType::kMaster, sql::kUpsertUser, name);

        if (result.AsSingleRow<int>() > 1) {
            user_type = domain::UserType::kKnown;
        }
    }

    return Response200{.body = domain::SayHelloTo(name, user_type)};
}

}  // namespace broker::views::hello_postgres::create
