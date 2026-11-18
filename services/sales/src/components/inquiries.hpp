#pragma once
#include <clients/counterparties/client.hpp>
#include <clients/organizations/client.hpp>
#include <clients/transport_locations/client.hpp>
#include <models/shipment_creation_context.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <string_view>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace sales {
class InquiriesComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "sales-inquiries";
    InquiriesComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    models::Inquiry SaveInquiry(const boost::uuids::uuid& organization,
        const platform::auth::Identity& identity, const models::SaveInquiryRequest& request) const;
    models::Inquiry FindInquiry(
        const boost::uuids::uuid& organization, const boost::uuids::uuid& id) const;
    models::ShipmentCreationContext PrepareShipmentCreation(const boost::uuids::uuid& organization,
        std::string_view access_token, const models::Inquiry& inquiry) const;
    models::Inquiry StartShipmentCreation(const boost::uuids::uuid& organization,
        const platform::auth::Identity& actor, const models::CreateShipmentRequest& request,
        const models::ShipmentCreationContext& creation) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
    clients::counterparties::Client counterparties_;
    clients::organizations::Client organizations_;
    clients::transport_locations::Client transport_locations_;
};
}  // namespace sales

template <>
inline constexpr bool userver::components::kHasValidate<sales::InquiriesComponent> = true;
