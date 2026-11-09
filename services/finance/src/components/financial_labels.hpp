#pragma once
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace finance {
class FinancialLabelsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "finance-financial-labels";
    FinancialLabelsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();

    struct FilterValuesResult {
        std::vector<models::RegistryField> fields;
        std::optional<std::string> next_after;
        std::vector<models::RegistryValue> values;
    };

    struct ListResult {
        std::vector<models::FinancialLabel> items;
        std::optional<models::FinancialLabelRegistryCursor> next_cursor;
    };

    FilterValuesResult FilterValues(const models::FinancialLabelFilterValuesRequest&,
        userver::server::request::RequestContext&) const;
    models::FinancialLabel Save(
        const models::SaveFinancialLabelRequest&, userver::server::request::RequestContext&) const;
    models::FinancialLabel Get(
        const models::GetFinancialLabelRequest&, userver::server::request::RequestContext&) const;
    ListResult List(
        const models::ListFinancialLabelsRequest&, userver::server::request::RequestContext&) const;

  private:
    struct Dependencies {
        userver::storages::postgres::ClusterPtr pg_cluster;
    };

    Dependencies deps_;
};
}  // namespace finance

template <>
inline constexpr bool userver::components::kHasValidate<finance::FinancialLabelsComponent> = true;
