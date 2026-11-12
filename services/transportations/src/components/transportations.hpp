#pragma once
#include <models/shipment_context.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace transportations {
class TransportationsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "transportations-transportations";
    TransportationsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::Transportation FindTransportation(
        const std::string& organization, const std::string& id) const;
    models::TransportationHistoryResult GetTransportationHistory(
        const std::string& organization, const models::ListHistoryRequest&) const;
    models::Transportation CreateTransportation(const std::string& organization,
        const platform::auth::Identity&, const models::CreateTransportationRequest&,
        const models::ShipmentContext&) const;
    models::Transportation UpdateTransportation(const std::string& organization,
        const platform::auth::Identity&, const models::UpdateTransportationRequest&,
        const models::ShipmentContext&) const;
    models::ContainerFactsResult GetContainerFacts(
        const std::string& organization, const models::ContainerFactsRequest&) const;
    models::EnsureTransportationsResult CreateTransportationsFromCommand(
        const models::EnsureCreatedRequest&, const platform::auth::Identity&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace transportations

template <>
inline constexpr bool userver::components::kHasValidate<transportations::TransportationsComponent> =
    true;
