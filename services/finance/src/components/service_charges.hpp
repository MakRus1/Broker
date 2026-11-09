#pragma once
#include <clients/contracts/client.hpp>
#include <clients/counterparties/client.hpp>
#include <clients/shipments/client.hpp>
#include <clients/transportations/client.hpp>
#include <components/exchange_rates.hpp>
#include <openapi/models.hpp>
#include <postgresql/types.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace finance {
class ServiceChargesComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "finance-service-charges";
    ServiceChargesComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();

    struct ListResult {
        std::vector<models::ServiceChargeInvoice> invoices;
        std::vector<models::ServiceCharge> items;
        std::optional<std::string> next_after;
        models::ReportingSummary reporting;
        models::ServiceChargeSummary summary;
    };

    struct RegistryResult {
        std::vector<models::ServiceChargeInvoice> invoices;
        std::vector<models::ServiceCharge> items;
        std::optional<models::ServiceChargeRegistryCursor> next_cursor;
    };

    struct FilterValuesResult {
        std::vector<models::RegistryField> fields;
        std::optional<std::string> next_after;
        std::vector<models::RegistryValue> values;
    };

    models::ServiceCharge Save(
        const models::SaveServiceChargeRequest&, userver::server::request::RequestContext&) const;
    ListResult List(
        const models::ListServiceChargesRequest&, userver::server::request::RequestContext&) const;
    models::ServiceCharge Get(
        const models::GetServiceChargeRequest&, userver::server::request::RequestContext&) const;
    bool ApplyImport(const models::ImportServiceChargesRequest&,
        userver::server::request::RequestContext&) const;
    RegistryResult Registry(const models::ServiceChargeRegistryRequest&,
        userver::server::request::RequestContext&) const;
    FilterValuesResult FilterValues(
        models::ServiceChargeFilterValuesRequest, userver::server::request::RequestContext&) const;

  private:
    struct ServiceChargeParties {
        std::string CounterpartyName;
        std::optional<std::string> ContractNumber;
    };

    models::ReportingSummary BuildReporting(const std::string&,
        const userver::storages::postgres::ResultSet&, bool transportation_scope) const;

    pgtypes::ServiceChargeV1 PrepareServiceCharge(const models::SaveServiceChargeRequest&,
        const models::FinancialLabel&, const ServiceChargeParties&, const Rate&) const;

    void ValidateServiceChargeScope(
        const userver::server::request::RequestContext&, const models::ServiceChargeScope&) const;
    ServiceChargeParties ValidateServiceChargeParties(
        const userver::server::request::RequestContext&,
        const models::SaveServiceChargeRequest&) const;

    struct Dependencies {
        clients::contracts::Client contracts;
        clients::counterparties::Client counterparties;
        clients::shipments::Client shipments;
        clients::transportations::Client transportations;
        ExchangeRates& exchange_rates;
        userver::storages::postgres::ClusterPtr pg_cluster;
    };

    Dependencies deps_;
};
}  // namespace finance

template <>
inline constexpr bool userver::components::kHasValidate<finance::ServiceChargesComponent> = true;
