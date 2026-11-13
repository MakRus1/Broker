#include <components/payments.hpp>
#include <permissions/permissions.hpp>
#include <platform/auth/identity.hpp>
#include <utility>
#include <views/api/v1/payments/get/view.hpp>

namespace finance::views::api::v1::payments::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    const auto& identity = platform::auth::GetIdentity(context);
    const auto organization = platform::auth::GetOrganization(identity);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_PAYMENTS);

    auto result = deps.payments->FindPayment(organization, request.body);
    return Response200{.allocated_minor = std::move(result.allocated_minor),
        .allocations = std::move(result.allocations),
        .payment = std::move(result.payment),
        .unallocated_minor = std::move(result.unallocated_minor)};
}
}  // namespace finance::views::api::v1::payments::get
