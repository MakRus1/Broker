#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/counterparties/list/view.hpp>

namespace counterparties::views::api::v1::counterparties::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::counterparties::helpers;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        const auto organization = ResolveOrganization(*deps.pg_cluster, identity);
        const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster,
            sql::kListCounterparties, organization, request.body.after, request.body.limit);
        return Response200{.items = Convert(rows)};
    } catch (const ::counterparties::ApiError& error) {
        return Response403{.error = error.Code, .message = error.what()};
    }
}
}  // namespace counterparties::views::api::v1::counterparties::list
