#include <helpers/invoices.hpp>
#include <permissions/permissions.hpp>
#include <views/api/v1/invoices/get/view.hpp>

namespace finance::views::api::v1::invoices::get {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::finance::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_READ_INVOICES);
    const auto organization = platform::auth::GetOrganization(identity);
    auto transaction = deps.pg_cluster->Begin(pg::ClusterHostType::kSlave,
        pg::TransactionOptions{
            pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
    const auto result = LoadInvoice(transaction, organization, request.body.id);
    transaction.Commit();
    return Response200{.charges = result.charges, .invoice = result.invoice};
}
}  // namespace finance::views::api::v1::invoices::get
