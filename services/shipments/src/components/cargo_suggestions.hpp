#pragma once
#include <boost/uuid/uuid.hpp>
#include <string>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <vector>

namespace shipments {
class CargoSuggestionsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "shipments-cargo-suggestions";
    CargoSuggestionsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    std::vector<std::string> FindSuggestions(const boost::uuids::uuid& organization,
        const boost::uuids::uuid& customer, const std::string& query) const;
    bool RemoveSuggestion(const boost::uuids::uuid& organization,
        const boost::uuids::uuid& customer, const std::string& value) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace shipments

template <>
inline constexpr bool userver::components::kHasValidate<shipments::CargoSuggestionsComponent> =
    true;
