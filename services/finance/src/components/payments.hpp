#pragma once
#include <models/available_payments.hpp>
#include <models/payment_filter_values_result.hpp>
#include <models/payment_invoice_candidates_result.hpp>
#include <models/payment_list_result.hpp>
#include <models/pg/payment_raw.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace finance {
class InvoicesComponent;

class PaymentsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "finance-payments";
    PaymentsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();

    models::PaymentDetails SavePayment(const std::string& organization,
        const platform::auth::Identity& identity, const models::SavePaymentRequest& request,
        models::pg::PaymentRaw value) const;
    models::PaymentDetails FindPayment(
        const std::string& organization, const models::InvoiceBalanceRequest& request) const;
    models::PaymentListResult ListPayments(
        const std::string& organization, const models::ListPaymentsRequest& request) const;
    models::PaymentFilterValuesResult GetPaymentFilterValues(
        const std::string& organization, const models::PaymentFilterValuesRequest& request) const;
    models::PaymentInvoiceCandidatesResult ListInvoiceCandidates(const std::string& organization,
        const models::PaymentInvoiceCandidatesRequest& request) const;
    models::PaymentInvoice GetInvoiceBalance(
        const std::string& organization, const models::InvoiceBalanceRequest& request) const;
    models::AvailablePayments ListPaymentCandidates(
        const std::string& organization, const models::PaymentCandidatesRequest& request) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
    InvoicesComponent& invoices_;
};
}  // namespace finance

template <>
inline constexpr bool userver::components::kHasValidate<finance::PaymentsComponent> = true;
