#pragma once
#include <models/registry_page.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace sales {
class SalesRegistry final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "sales-registry";
    SalesRegistry(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::RegistryPage<models::SalesCard> ListCards(
        const std::string& organization, const models::ListSalesRequest&) const;
    models::RegistryPage<models::Inquiry> ListInquiries(
        const std::string& organization, const models::ListSalesRequest&) const;
    models::RegistryFilterValues GetCardsFilterValues(
        const std::string& organization, const models::SalesFilterValuesRequest&) const;
    models::RegistryFilterValues GetInquiriesFilterValues(
        const std::string& organization, const models::SalesFilterValuesRequest&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace sales

template <> inline constexpr bool userver::components::kHasValidate<sales::SalesRegistry> = true;
