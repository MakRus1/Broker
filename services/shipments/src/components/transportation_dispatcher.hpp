#pragma once
#include <deps.hpp>
#include <platform/auth/service_token.hpp>
#include <userver/components/component_base.hpp>
#include <userver/utils/periodic_task.hpp>

namespace shipments {
class TransportationDispatcher final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "transportation-dispatcher";
    TransportationDispatcher(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    ~TransportationDispatcher() override;
    static userver::yaml_config::Schema GetStaticConfigSchema();

  private:
    void Step();
    bool DeliverRouteProjection();
    Deps deps_;
    platform::auth::ServiceTokenProvider& tokens_;
    userver::utils::PeriodicTask task_;
    userver::utils::PeriodicTask route_task_;
};
}  // namespace shipments

template <>
inline constexpr bool userver::components::kHasValidate<shipments::TransportationDispatcher> = true;
