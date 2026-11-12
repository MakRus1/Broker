#include <components/rfq.hpp>
#include <platform/auth/identity.hpp>
#include <views/api/v1/sales/rfqs/history/view.hpp>

namespace sales::views::api::v1::sales::rfqs::history {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    return Response200{.items = deps.rfq->GetRfqHistory(organization, request.body)};
}
}  // namespace sales::views::api::v1::sales::rfqs::history
