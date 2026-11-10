#pragma once
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace transport_locations {
class RailwayStationsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "railway-stations";
    RailwayStationsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::StationListResult List(const models::ListStationsRequest& request) const;
    models::RailwayStation Save(const models::SaveStationRequest& request) const;
    void Archive(const std::string& id) const;
    std::vector<models::RailwayStation> GetBatch(const std::vector<std::string>& ids) const;
    std::vector<models::RailwayStation> Search(const std::string& query) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace transport_locations

template <>
inline constexpr bool
    userver::components::kHasValidate<transport_locations::RailwayStationsComponent> = true;
