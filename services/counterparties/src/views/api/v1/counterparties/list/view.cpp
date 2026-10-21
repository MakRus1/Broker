#include <helpers/converters.hpp>
#include <helpers/database.hpp>
#include <platform/auth/principal.hpp>
#include <views/api/v1/counterparties/list/view.hpp>

namespace counterparties::views::api::v1::counterparties::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::counterparties::helpers;
    try {
        const auto& principal = platform::auth::GetPrincipal(context);
        const auto tenant = ResolveTenant(*deps.pg_cluster, principal);
        const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster,
            sql::kListCounterparties, tenant, request.body.after, request.body.limit);
        return Response200{.items = Convert(rows)};
    } catch (const ::counterparties::ApiError& error) {
        return Response403{.error = error.Code, .message = error.what()};
    }
}
}  // namespace counterparties::views::api::v1::counterparties::list
