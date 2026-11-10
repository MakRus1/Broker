#pragma once
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace shipments {
class TransportationOperationsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "shipments-transportation-operations";
    TransportationOperationsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::TransportationOperationsCreateResult Create(
        const models::CreateOperationRequest& request,
        userver::server::request::RequestContext& context) const;
    models::TransportationOperationsGetResult Get(const models::GetOperationRequest& request,
        userver::server::request::RequestContext& context) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
    models::CreationOperation RequestTransportations(
        const platform::auth::Identity&, const models::CreateOperationRequest&) const;
};
}  // namespace shipments

template <>
inline constexpr bool
    userver::components::kHasValidate<shipments::TransportationOperationsComponent> = true;
