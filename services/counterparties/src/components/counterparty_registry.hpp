#pragma once
#include <models/counterparty_filter_values_result.hpp>
#include <models/counterparty_list_result.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace counterparties {
class CounterpartyRegistry final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "counterparties-registry";
    CounterpartyRegistry(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    models::CounterpartyListResult ListCounterparties(
        const boost::uuids::uuid& organization, const models::ListCounterpartiesRequest&) const;
    models::CounterpartyFilterValuesResult GetCounterpartyFilterValues(
        const boost::uuids::uuid& organization,
        const models::CounterpartyFilterValuesRequest&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace counterparties

template <>
inline constexpr bool userver::components::kHasValidate<counterparties::CounterpartyRegistry> =
    true;
