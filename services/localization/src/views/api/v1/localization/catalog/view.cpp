#include <helpers/catalog.hpp>
#include <views/api/v1/localization/catalog/view.hpp>

namespace localization::views::api::v1::localization::catalog {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::localization::helpers;
    CheckOrganization(deps, context, request.body.organization_id);
    const auto rows = deps.pg_cluster->Execute(pg::ClusterHostType::kMaster, sql::kGetCatalog,
        request.body.organization_id, EnumValue(request.body.locale));
    const auto messages = rows.AsSingleRow<userver::formats::json::Value>();
    Response200 response;
    for (auto it = messages.begin(); it != messages.end(); ++it) {
        if (it->IsString())
            response.messages.push_back({.key = it.GetName(), .value = it->As<std::string>()});
    }
    return response;
}
}  // namespace localization::views::api::v1::localization::catalog
