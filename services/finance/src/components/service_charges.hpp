#pragma once
#include <components/exchange_rates.hpp>
#include <models/pg/service_charge_raw.hpp>
#include <models/service_charge_filter_values_result.hpp>
#include <models/service_charge_list_result.hpp>
#include <models/service_charge_parties.hpp>
#include <models/service_charge_registry_result.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace finance {
class InvoicesComponent;

class ServiceChargesComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "finance-service-charges";
    ServiceChargesComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();

    models::ServiceCharge SaveServiceCharge(const std::string& organization,
        const platform::auth::Identity& identity, const models::SaveServiceChargeRequest& request,
        const models::ServiceChargeParties& parties, const models::ConversionRate& exchange_rate,
        bool can_write_invoices) const;
    models::ServiceChargeListResult ListServiceCharges(
        const std::string& organization, const models::ListServiceChargesRequest& request) const;
    models::ServiceCharge FindServiceCharge(
        const std::string& organization, const models::GetServiceChargeRequest& request) const;
    bool ApplyServiceChargeImport(const platform::auth::Identity& identity,
        const models::ImportServiceChargesRequest& request) const;
    models::ServiceChargeRegistryResult ListServiceChargeRegistry(
        const std::string& organization, const models::ServiceChargeRegistryRequest& request) const;
    models::ServiceChargeFilterValuesResult GetServiceChargeFilterValues(
        const std::string& organization, models::ServiceChargeFilterValuesRequest request) const;

  private:
    models::ReportingSummary BuildReporting(const std::string&,
        const userver::storages::postgres::ResultSet&, bool transportation_scope) const;

    userver::storages::postgres::ClusterPtr cluster_;
    ExchangeRates& exchange_rates_;
    InvoicesComponent& invoices_;
};
}  // namespace finance

template <>
inline constexpr bool userver::components::kHasValidate<finance::ServiceChargesComponent> = true;
