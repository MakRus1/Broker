#pragma once
#include <clients/transportations/client.hpp>
#include <components/settings.hpp>
#include <models/container_plan.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace shipments {
class ContainerPlanComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "shipments-container-plan";
    ContainerPlanComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::ShipmentItem AddPlannedContainer(
        const models::AddItemRequest&, const platform::auth::Identity&) const;
    ContainerPlanSnapshot LoadPlan(
        const std::string& organization, const std::string& shipment_id) const;
    models::ContainerPlanListResult BuildPlanPage(const ContainerPlanSnapshot& snapshot,
        const clients::transportations::ApiV1TransportationsContainerFact::Response& facts,
        const models::ListItemsRequest& request) const;
    models::ShipmentItem UpdatePlannedContainer(
        const models::UpdateItemRequest&, const platform::auth::Identity&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
    SettingsComponent& settings_;
};
}  // namespace shipments

template <>
inline constexpr bool userver::components::kHasValidate<shipments::ContainerPlanComponent> = true;
