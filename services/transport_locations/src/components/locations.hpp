#pragma once
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace transport_locations {
class LocationsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "route-locations";
    LocationsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::LocationListResult List(const models::ListLocationsRequest& request) const;
    models::TransportLocation Save(const models::SaveLocationRequest& request) const;
    void Archive(const std::string& id) const;
    std::vector<models::TransportLocation> GetBatch(const std::vector<std::string>& ids) const;
    std::vector<models::TransportLocation> Search(
        models::TransportLocationKind kind, const std::optional<std::string>& query) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace transport_locations

template <>
inline constexpr bool userver::components::kHasValidate<transport_locations::LocationsComponent> =
    true;
