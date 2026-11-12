#pragma once
#include <clients/counterparties/client.hpp>
#include <clients/finance/client.hpp>
#include <clients/organizations/client.hpp>
#include <clients/transport_locations/client.hpp>
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>

namespace sales {
class SalesDirectory final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "sales-directory";
    SalesDirectory(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    clients::counterparties::models::Counterparty GetCounterparty(
        std::string_view token, const std::string& id) const;
    void ValidateSalesParties(const std::string& organization, std::string_view token,
        const std::string& customer, const std::string& manager) const;
    void RequireAdministrator(const std::string& organization, std::string_view token) const;
    void ResolveShipmentRoute(const std::string& organization, std::string_view token,
        std::vector<models::ShipmentRouteSegment>&, const std::optional<std::string>& logist) const;
    models::RfqInput ResolveSupplierQuotes(const std::string& organization, std::string_view token,
        const models::Inquiry&, models::RfqInput input) const;

  private:
    clients::counterparties::Client counterparties_;
    clients::finance::Client finance_;
    clients::organizations::Client organizations_;
    clients::transport_locations::Client transport_locations_;
};
}  // namespace sales

template <> inline constexpr bool userver::components::kHasValidate<sales::SalesDirectory> = true;
