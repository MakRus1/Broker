#pragma once
#include <userver/clients/http/client.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/utils/periodic_task.hpp>

namespace transport_locations {
class RailwaySync final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "railway-sync";
    RailwaySync(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    ~RailwaySync() override;
    static userver::yaml_config::Schema GetStaticConfigSchema();

  private:
    void Sync();
    userver::clients::http::Client& http_;
    userver::storages::postgres::ClusterPtr pg_;
    std::string url_;
    std::chrono::milliseconds timeout_;
    std::size_t minimum_;
    userver::utils::PeriodicTask task_;
};
}  // namespace transport_locations

template <>
inline constexpr bool userver::components::kHasValidate<transport_locations::RailwaySync> = true;
