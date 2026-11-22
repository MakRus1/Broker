#pragma once
#include <models/shipment_context.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace transportations {
void ValidateTransportationDetails(models::TransportationType, const std::optional<std::string>&,
    const std::optional<models::ContainerOwnership>&, const std::optional<std::string>&);

class TransportationsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "transportations-transportations";
    TransportationsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    models::Transportation FindTransportation(
        const boost::uuids::uuid& organization, const boost::uuids::uuid& id) const;
    models::TransportationHistoryResult GetTransportationHistory(
        const boost::uuids::uuid& organization, const models::ListHistoryRequest&) const;
    models::Transportation CreateTransportation(const boost::uuids::uuid& organization,
        const platform::auth::Identity&, const models::CreateTransportationRequest&,
        const models::ShipmentContext&) const;
    models::Transportation UpdateTransportation(const boost::uuids::uuid& organization,
        const platform::auth::Identity&, const models::UpdateTransportationRequest&,
        const models::ShipmentContext&) const;
    models::ContainerFactsResult GetContainerFacts(
        const boost::uuids::uuid& organization, const models::ContainerFactsRequest&) const;
    models::EnsureTransportationsResult CreateTransportationsFromCommand(
        const models::EnsureCreatedRequest&, const platform::auth::Identity&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace transportations

template <>
inline constexpr bool userver::components::kHasValidate<transportations::TransportationsComponent> =
    true;
