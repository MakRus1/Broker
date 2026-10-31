#include <platform/browser/session.hpp>
#include <views/api/admin/contracts/numbering/update/view.hpp>

namespace admin_bff::views::api::admin::contracts::numbering::update {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& session = platform::browser::GetBrowserSession(context);
    const auto& body = request.body;
    const auto result = deps.contracts.ApiAdminContractsNumberingUpdate(
        {.body = {.expected_revision = body.expected_revision,
             .number_template = body.number_template,
             .organization_id = body.organization_id,
             .reset_number_yearly = body.reset_number_yearly}},
        session.Access);
    return Response200{.number_template = result.number_template,
        .reset_number_yearly = result.reset_number_yearly,
        .revision = result.revision};
}
}  // namespace admin_bff::views::api::admin::contracts::numbering::update
