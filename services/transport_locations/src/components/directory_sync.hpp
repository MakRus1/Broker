#pragma once
#include <boost/uuid/uuid.hpp>
#include <chrono>
#include <functional>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace transport_locations {
struct SyncPolicy {
    bool Enabled;
    int PeriodSeconds;
    std::chrono::milliseconds HttpTimeout;
    int HttpRetries;
    int RetryInitialSeconds;
    int RetryMaxSeconds;
};

class DirectorySyncComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "directory-sync";
    DirectorySyncComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    void Run(const std::string& source, const SyncPolicy& policy, int requests,
        const std::function<void(const boost::uuids::uuid&)>& sync) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace transport_locations

template <>
inline constexpr bool
    userver::components::kHasValidate<transport_locations::DirectorySyncComponent> = true;
