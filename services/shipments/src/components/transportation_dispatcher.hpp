#pragma once
#include <deps.hpp>
#include <platform/auth/service_token.hpp>
#include <platform/outbox.hpp>
#include <postgresql/types.hpp>
#include <userver/components/component_base.hpp>

namespace shipments {
class TransportationDispatcher final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "transportation-dispatcher";
    TransportationDispatcher(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    ~TransportationDispatcher() override;
    static userver::yaml_config::Schema GetStaticConfigSchema();

  private:
    struct RouteDelivery {
        std::string Organization;
        std::string Shipment;
        std::int64_t Revision;
        userver::formats::json::Value Segments;
        std::string Lease;
    };

    pgtypes::CreationOperationV1 SendOperation(const pgtypes::DeliveryV2& delivery);
    void SendRouteProjection(const RouteDelivery& delivery);
    Deps deps_;
    platform::auth::ServiceTokenProvider& tokens_;
    platform::outbox::Worker task_;
    platform::outbox::Worker route_task_;
};
}  // namespace shipments

template <>
inline constexpr bool userver::components::kHasValidate<shipments::TransportationDispatcher> = true;
