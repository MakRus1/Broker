#pragma once
#include <models/invoice_candidates_result.hpp>
#include <models/invoice_filter_values_result.hpp>
#include <models/invoice_list_result.hpp>
#include <models/invoice_print_snapshot.hpp>
#include <models/pg/invoice_raw.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace finance {
class InvoicesComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "finance-invoices";
    InvoicesComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();

    models::InvoicePrintSnapshot ReadInvoicePrintSnapshot(
        const std::string& organization, const models::InvoicePrintRequest& request) const;
    models::InvoicePrintData SaveInvoicePrintSnapshot(
        const std::string& organization, const models::InvoicePrintData& data) const;
    models::InvoiceDetails SaveInvoice(const std::string& organization,
        const platform::auth::Identity& identity, const models::SaveInvoiceRequest& request,
        models::pg::InvoiceRaw value) const;
    models::InvoiceDetails FindInvoice(
        const std::string& organization, const models::GetInvoiceRequest& request) const;
    models::InvoiceListResult ListInvoices(
        const std::string& organization, const models::ListInvoicesRequest& request) const;
    models::InvoiceCandidatesResult ListServiceChargeCandidates(
        const std::string& organization, const models::ListInvoiceCandidatesRequest& request) const;
    models::InvoiceDetails IncludeServiceCharge(const std::string& organization,
        const platform::auth::Identity& identity,
        const models::AddInvoiceServiceChargeRequest& request) const;
    models::InvoiceDetails DetachServiceCharge(const std::string& organization,
        const platform::auth::Identity& identity,
        const models::DetachInvoiceServiceChargeRequest& request) const;
    models::InvoiceFilterValuesResult GetInvoiceFilterValues(
        const std::string& organization, const models::InvoiceFilterValuesRequest& request) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace finance

template <>
inline constexpr bool userver::components::kHasValidate<finance::InvoicesComponent> = true;
