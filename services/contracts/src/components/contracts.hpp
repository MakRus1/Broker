#pragma once
#include <clients/counterparties/client.hpp>
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace contracts {
class ContractsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "contracts-documents";
    ContractsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();

    struct FilterValuesResult {
        std::vector<models::RegistryField> fields;
        std::vector<models::RegistryValue> values;
        std::optional<std::string> next_after;
    };

    struct ListResult {
        std::vector<models::Contract> items;
        std::optional<models::ContractRegistryCursor> next_cursor;
    };

    models::Contract Save(
        const models::SaveContractRequest&, userver::server::request::RequestContext&) const;
    models::Contract Get(const std::string& id, userver::server::request::RequestContext&) const;
    ListResult List(
        const models::ListContractsRequest&, userver::server::request::RequestContext&) const;
    FilterValuesResult FilterValues(const models::ContractFilterValuesRequest&,
        userver::server::request::RequestContext&) const;

  private:
    void ValidateParties(
        const userver::server::request::RequestContext&, const models::SaveContractRequest&) const;
    clients::counterparties::Client counterparties_;
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace contracts

template <>
inline constexpr bool userver::components::kHasValidate<contracts::ContractsComponent> = true;
