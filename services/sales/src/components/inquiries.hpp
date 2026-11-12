#pragma once
#include <models/shipment_creation_context.hpp>
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace sales {
class InquiriesComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "sales-inquiries";
    InquiriesComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::Inquiry SaveInquiry(const std::string& organization,
        const platform::auth::Identity& identity, const models::SaveInquiryRequest& request) const;
    models::Inquiry FindInquiry(const std::string& organization, const std::string& id) const;
    std::vector<models::InquiryHistoryEntry> GetInquiryHistory(
        const std::string& organization, const models::HistoryRequest& request) const;
    models::Inquiry StartShipmentCreation(const std::string& organization,
        const platform::auth::Identity& actor, const models::CreateShipmentRequest& request,
        const models::ShipmentCreationContext& creation) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace sales

template <>
inline constexpr bool userver::components::kHasValidate<sales::InquiriesComponent> = true;
