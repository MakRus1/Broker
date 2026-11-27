#pragma once
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace shipments {
class VoyagesComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "voyages";
    VoyagesComponent(const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context);
    models::VoyageList ListVoyages(const boost::uuids::uuid& org) const;
    models::VoyageDetail GetVoyage(
        const boost::uuids::uuid& org, const boost::uuids::uuid& id) const;
    std::optional<std::vector<models::ShipmentVoyageLink>> GetConfirmedVoyages(
        userver::storages::postgres::Transaction& transaction,
        const boost::uuids::uuid& organization, const boost::uuids::uuid& shipment_id) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace shipments

template <>
inline constexpr bool userver::components::kHasValidate<shipments::VoyagesComponent> = true;
