#pragma once
#include <models/financial_label_filter_values_result.hpp>
#include <models/financial_label_list_result.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace finance {
class FinancialLabelsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "finance-financial-labels";
    FinancialLabelsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();

    models::FinancialLabelFilterValuesResult GetFinancialLabelFilterValues(
        const std::string& organization,
        const models::FinancialLabelFilterValuesRequest& request) const;
    models::FinancialLabel SaveFinancialLabel(const std::string& organization,
        const platform::auth::Identity& identity,
        const models::SaveFinancialLabelRequest& request) const;
    models::FinancialLabel FindFinancialLabel(
        const std::string& organization, const models::GetFinancialLabelRequest& request) const;
    models::FinancialLabelListResult ListFinancialLabels(
        const std::string& organization, const models::ListFinancialLabelsRequest& request) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace finance

template <>
inline constexpr bool userver::components::kHasValidate<finance::FinancialLabelsComponent> = true;
