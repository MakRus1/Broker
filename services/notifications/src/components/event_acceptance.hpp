#pragma once
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace notifications {
class EventAcceptanceComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "notifications-event-acceptance";
    EventAcceptanceComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    void AcceptEvent(const models::PublishedEvent&, const platform::auth::Identity&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace notifications

template <>
inline constexpr bool userver::components::kHasValidate<notifications::EventAcceptanceComponent> =
    true;
