#include <helpers/numbering.hpp>
#include <views/api/admin/contracts/numbering/get/view.hpp>

namespace contracts::views::api::admin::contracts::numbering::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& body = request.body;
    helpers::RequireNumberingAdministrator(deps, context, body.organization_id);
    const auto result = helpers::ConvertNumbering(deps.pg_cluster->Execute(
        helpers::pg::ClusterHostType::kMaster, sql::kGetNumbering, body.organization_id));
    return Response200{.number_template = result.number_template,
        .reset_number_yearly = result.reset_number_yearly,
        .revision = result.revision};
}
}  // namespace contracts::views::api::admin::contracts::numbering::get
