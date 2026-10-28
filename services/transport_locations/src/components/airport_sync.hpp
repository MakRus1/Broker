#pragma once
#include <platform/configs/runtime.hpp>
#include <userver/clients/http/client.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/utils/periodic_task.hpp>

namespace transport_locations {
class AirportSync final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "airport-sync";
    AirportSync(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    ~AirportSync() override;
    static userver::yaml_config::Schema GetStaticConfigSchema();

  private:
    void Sync();
    userver::clients::http::Client& http_;
    userver::storages::postgres::ClusterPtr pg_;
    std::string url_;
    platform::configs::Source configs_;
    std::size_t minimum_;
    userver::utils::PeriodicTask task_;
};
}  // namespace transport_locations

template <>
inline constexpr bool userver::components::kHasValidate<transport_locations::AirportSync> = true;
