#include <helpers/invoices.hpp>
#include <permissions/permissions.hpp>
#include <views/api/v1/invoices/add_service_charge/view.hpp>

namespace finance::views::api::v1::invoices::add_service_charge {
Response View::Handle(
    Request&& request, userver::server::request::RequestContext& context, Deps deps) {
    using namespace ::finance::helpers;
    const auto& identity = platform::auth::GetIdentity(context);
    platform::auth::CheckPermission(context, permissions::FINANCE_WRITE_INVOICES);
    const auto organization = platform::auth::GetOrganization(identity);
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    const auto charges = transaction.Execute(sql::kLockServiceCharges, organization,
        std::vector{boost::uuids::string_generator{}(request.body.service_charge_id)});
    if (charges.IsEmpty())
        throw ApiError<404>{"service_charge_not_found", "Услуга не найдена"};
    const auto charge = charges.AsSingleRow<pgtypes::ServiceChargeV1>();
    const auto result = IncludeServiceCharge(transaction, organization, identity, charge,
        request.body.invoice_id, request.body.expected_revision, request.body.previous_invoice_id);
    transaction.Commit();
    return Response200{.charges = result.charges, .invoice = result.invoice};
}
}  // namespace finance::views::api::v1::invoices::add_service_charge
