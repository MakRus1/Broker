#include <components/payments.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/payments/payment_candidates/view.hpp>

namespace finance::views::api::v1::payments::payment_candidates {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_PAYMENTS);

    auto result = deps.payments->ListPaymentCandidates(organization, request.body);
    return Response200{
        .items = std::move(result.items), .next_after = std::move(result.next_after)};
}
}  // namespace finance::views::api::v1::payments::payment_candidates
