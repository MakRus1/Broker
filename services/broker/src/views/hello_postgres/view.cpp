#include <views/hello_postgres/view.hpp>

#include <greeting.hpp>

#include <userver/storages/postgres/cluster.hpp>

namespace broker::views::hello_postgres {

Response View::Handle(
    Request&& request,
    userver::server::request::RequestContext& /*context*/,
    Deps deps) {
    const auto& name = request.name;
    auto user_type = UserType::kFirstTime;

    if (!name.empty()) {
        const auto result = deps.pg_cluster->Execute(
            userver::storages::postgres::ClusterHostType::kMaster,
            "INSERT INTO hello_schema.users(name, count) VALUES($1, 1) "
            "ON CONFLICT (name) "
            "DO UPDATE SET count = users.count + 1 "
            "RETURNING users.count",
            name);

        if (result.AsSingleRow<int>() > 1) {
            user_type = UserType::kKnown;
        }
    }

    return PlainTextResponse{.body = SayHelloTo(name, user_type)};
}

}  // namespace broker::views::hello_postgres
