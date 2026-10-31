#include <platform/browser/session.hpp>
#include <views/api/admin/contracts/numbering/get/view.hpp>

namespace admin_bff::views::api::admin::contracts::numbering::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto& body = request.body;
    const auto result = deps.contracts.ApiAdminContractsNumberingGet(
        {.body = {.organization_id = body.organization_id}}, session.Access);
    return Response200{.number_template = result.number_template,
        .reset_number_yearly = result.reset_number_yearly,
        .revision = result.revision};
}
}  // namespace admin_bff::views::api::admin::contracts::numbering::get
