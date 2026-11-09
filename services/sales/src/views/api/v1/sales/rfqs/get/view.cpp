#include <components/rfq.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/sales/rfqs/get/view.hpp>

namespace sales::views::api::v1::sales::rfqs::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    return Response200{.item = deps.rfq->Get(organization, request.body.id)};
}
}  // namespace sales::views::api::v1::sales::rfqs::get
