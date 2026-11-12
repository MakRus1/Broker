#pragma once
#include <clients/finance/client.hpp>
#include <clients/notifications/client.hpp>
#include <clients/shipments/client.hpp>
#include <events/shipment_command.hpp>
#include <platform/auth/service_token.hpp>
#include <platform/outbox/component.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace sales {
class ShipmentDispatcher final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "shipment-dispatcher";
    ShipmentDispatcher(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();

  private:
    void TransferProposal(
        const ShipmentCommand&, const std::string& shipment_id, const std::string& token) const;

    clients::finance::Client finance_;
    clients::shipments::Client shipments_;
    clients::notifications::Client notifications_;
    userver::storages::postgres::ClusterPtr cluster_;
    platform::auth::ServiceTokenProvider& tokens_;
};
}  // namespace sales

template <>
inline constexpr bool userver::components::kHasValidate<sales::ShipmentDispatcher> = true;
