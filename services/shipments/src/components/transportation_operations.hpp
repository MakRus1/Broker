#pragma once
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace shipments {
class TransportationOperationsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "shipments-transportation-operations";
    TransportationOperationsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::CreationOperation GetCreationOperation(
        const std::string& organization, const std::string& operation_id) const;
    models::CreationOperation RequestTransportations(
        const platform::auth::Identity&, const models::CreateOperationRequest&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace shipments

template <>
inline constexpr bool
    userver::components::kHasValidate<shipments::TransportationOperationsComponent> = true;
