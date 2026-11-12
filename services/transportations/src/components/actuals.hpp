#pragma once
#include <components/actual_changes.hpp>
#include <components/route_integrations.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <string_view>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace transportations {
class ActualsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "transportations-actuals";
    ActualsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::Transportation RecordActual(const std::string& organization,
        const platform::auth::Identity&, std::string_view access_token,
        const models::RecordActualRequest&, const models::ShipmentContext&) const;
    models::Transportation CorrectActual(const std::string& organization,
        const platform::auth::Identity&, std::string_view access_token,
        const models::CorrectActualRequest&, const models::ShipmentContext&) const;

  private:
    template <typename Request>
    models::Transportation ApplyActualChange(const Request&, const std::string& organization,
        const platform::auth::Identity&, std::string_view access_token,
        const models::ShipmentContext&, route_rules::ActualChange) const;
    userver::storages::postgres::ClusterPtr cluster_;
    RouteIntegrations& integrations_;
};
}  // namespace transportations

template <>
inline constexpr bool userver::components::kHasValidate<transportations::ActualsComponent> = true;
