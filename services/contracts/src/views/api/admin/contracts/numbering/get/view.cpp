#include <components/numbering.hpp>
#include <views/api/admin/contracts/numbering/get/view.hpp>

namespace contracts::views::api::admin::contracts::numbering::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto result = deps.numbering->Get(request.body.organization_id, context);
    return Response200{.number_template = result.number_template,
        .reset_number_yearly = result.reset_number_yearly,
        .revision = result.revision};
}
}  // namespace contracts::views::api::admin::contracts::numbering::get
