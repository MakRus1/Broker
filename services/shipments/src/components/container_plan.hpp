#pragma once
#include <clients/transportations/client.hpp>
#include <components/settings.hpp>
#include <models/container_plan.hpp>
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace shipments {
class ContainerPlanComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "shipments-container-plan";
    ContainerPlanComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::ContainerPlanAddResult Add(const models::AddItemRequest& request,
        userver::server::request::RequestContext& context) const;
    models::ContainerPlanListResult List(const models::ListItemsRequest& request,
        userver::server::request::RequestContext& context) const;
    models::ContainerPlanUpdateResult Update(const models::UpdateItemRequest& request,
        userver::server::request::RequestContext& context) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
    SettingsComponent& settings_;
    clients::transportations::Client transportations_;
    ContainerPlan ReadContainerPlan(const userver::server::request::RequestContext&,
        const std::string&, const std::vector<pgtypes::ShipmentItemV1>&,
        const std::vector<pgtypes::PendingContainerCreationV1>&) const;
};
}  // namespace shipments

template <>
inline constexpr bool userver::components::kHasValidate<shipments::ContainerPlanComponent> = true;
