#pragma once
#include <models/invoice_candidates_result.hpp>
#include <models/invoice_filter_values_result.hpp>
#include <models/invoice_list_result.hpp>
#include <models/invoice_print_snapshot.hpp>
#include <models/pg/invoice_raw.hpp>
#include <models/pg/service_charge_raw.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/transaction.hpp>

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

    // Используют транзакцию вызывающего сценария.
    models::InvoiceDetails FindInvoice(userver::storages::postgres::Transaction& transaction,
        const std::string& organization, const std::string& id) const;

    models::InvoiceDetails IncludeServiceCharge(
        userver::storages::postgres::Transaction& transaction, const std::string& organization,
        const platform::auth::Identity& identity, const models::pg::ServiceChargeRaw& charge,
        const std::string& invoice_id, std::int64_t expected_revision,
        const std::optional<std::string>& previous_invoice_id = std::nullopt) const;

    void UpdateServiceChargeSnapshot(userver::storages::postgres::Transaction& transaction,
        const std::string& organization, const platform::auth::Identity& identity,
        const boost::uuids::uuid& invoice_id, const models::pg::ServiceChargeRaw& charge) const;

  private:
    void ValidateInvoiceServiceCharge(
        const models::pg::ServiceChargeRaw& charge, const models::pg::InvoiceRaw& invoice) const;

    void ValidateInvoiceSettlement(userver::storages::postgres::Transaction& transaction,
        const std::string& organization, const models::pg::InvoiceRaw& invoice) const;

    void RemoveInvoiceServiceCharge(userver::storages::postgres::Transaction& transaction,
        const std::string& organization, const boost::uuids::uuid& invoice_id,
        const boost::uuids::uuid& service_charge_id,
        const platform::auth::Identity& identity) const;

    void CheckInvoiceAssignment(userver::storages::postgres::Transaction& transaction,
        const std::string& organization, const boost::uuids::uuid& service_charge_id,
        const std::optional<std::string>& previous) const;

    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace finance

template <>
inline constexpr bool userver::components::kHasValidate<finance::InvoicesComponent> = true;
