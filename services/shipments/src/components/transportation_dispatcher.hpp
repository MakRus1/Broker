#pragma once
#include <deps.hpp>
#include <platform/auth/service_token.hpp>
#include <platform/outbox/component.hpp>
#include <postgresql/types.hpp>
#include <userver/components/component_base.hpp>

namespace shipments {
class TransportationDispatcher final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "transportation-dispatcher";
    TransportationDispatcher(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();

  private:
    pgtypes::CreationOperationV1 SendOperation(const platform::outbox::Message&);
    void SendRouteProjection(const platform::outbox::Message&);
    Deps deps_;
    platform::auth::ServiceTokenProvider& tokens_;
};
}  // namespace shipments

template <>
inline constexpr bool userver::components::kHasValidate<shipments::TransportationDispatcher> = true;
