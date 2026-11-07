#include <helpers/settings.hpp>
#include <views/api/admin/sales/settings/get/view.hpp>

namespace sales::views::api::admin::sales::settings::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::sales::helpers;
    RequireAdministrator(deps, context, request.body.organization_id);
    const auto result = Settings(deps.pg_cluster->Execute(
        pg::ClusterHostType::kMaster, sql::kGetSettings, request.body.organization_id));
    return Response200{.inquiry_number_template = result.inquiry_number_template,
        .reset_number_yearly = result.reset_number_yearly,
        .revision = result.revision,
        .stages = result.stages};
}
}  // namespace sales::views::api::admin::sales::settings::get
