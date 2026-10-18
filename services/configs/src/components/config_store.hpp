#pragma once

#include <platform/configs/runtime.hpp>
#include <userver/components/component_base.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace configs {

struct Revision {
    std::int64_t Id;
    platform::configs::Json Values;
};
class RevisionConflict : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};
class RevisionNotFound : public std::runtime_error {
  public:
    using std::runtime_error::runtime_error;
};

class ConfigStore final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "config-store";
    ConfigStore(const userver::components::ComponentConfig&,
                const userver::components::ComponentContext&);
    Revision Current() const;
    Revision Publish(std::int64_t expected, const platform::configs::Json& patch,
                     std::optional<std::int64_t> rollback = {}) const;
    platform::configs::Json History() const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};

}  // namespace configs
