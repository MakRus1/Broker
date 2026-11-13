#include <components/numbering.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/admin/contracts/numbering/get/view.hpp>

namespace contracts::views::api::admin::contracts::numbering::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    deps.numbering->RequireAdministrator(
        request.body.organization_id, platform::auth::GetAccessToken(context));

    const auto result = deps.numbering->GetNumbering(request.body.organization_id);
    return Response200{.number_template = result.number_template,
        .reset_number_yearly = result.reset_number_yearly,
        .revision = result.revision};
}
}  // namespace contracts::views::api::admin::contracts::numbering::get
