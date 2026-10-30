#include <helpers/payments.hpp>
#include <permissions/permissions.hpp>
#include <views/api/v1/payments/get/view.hpp>

namespace finance::views::api::v1::payments::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::finance::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_PAYMENTS);
    const auto organization = platform::auth::GetOrganization(identity);
    auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster,
        pg::TransactionOptions{
            pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
    const auto result = LoadPayment(transaction, organization, request.body.id);
    transaction.Commit();
    return Response200{.allocated_minor = result.allocated_minor,
        .allocations = result.allocations,
        .payment = result.payment,
        .unallocated_minor = result.unallocated_minor};
}
}  // namespace finance::views::api::v1::payments::get
