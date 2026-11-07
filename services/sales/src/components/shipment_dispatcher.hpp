#pragma once
#include <deps.hpp>
#include <platform/auth/service_token.hpp>
#include <platform/outbox/component.hpp>
#include <userver/components/component_base.hpp>

namespace sales {
class ShipmentDispatcher final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "shipment-dispatcher";
    ShipmentDispatcher(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();

  private:
    Deps deps_;
    platform::auth::ServiceTokenProvider& tokens_;
};
}  // namespace sales

template <>
inline constexpr bool userver::components::kHasValidate<sales::ShipmentDispatcher> = true;
