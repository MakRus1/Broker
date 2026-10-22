#include <helpers/details.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/counterparties/list/view.hpp>

namespace counterparties::views::api::v1::counterparties::list {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::counterparties::helpers;
    try {
        const auto& identity = platform::auth::GetIdentity(context);
        const auto organization = platform::auth::GetOrganization(identity);
        std::optional<std::string> role;
        if (request.body.role.has_value())
            role = models::EnumValue(request.body.role.value());
        const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster,
            sql::kListCounterparties, organization, request.body.after, request.body.limit,
            userver::utils::text::Trim(request.body.query), role,
            models::EnumValue(request.body.status));
        return Response200{.items = Convert(rows)};
    } catch (const ::counterparties::ApiError& error) {
        switch (error.Status) {
        case 400:
            return Response400{.error = error.Code, .message = error.what()};
        case 403:
            return Response403{.error = error.Code, .message = error.what()};
        case 404:
            return Response404{.error = error.Code, .message = error.what()};
        case 409:
            return Response409{.error = error.Code, .message = error.what()};
        default:
            throw;
        }
    }
}
}  // namespace counterparties::views::api::v1::counterparties::list
