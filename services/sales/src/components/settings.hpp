#pragma once
#include <openapi/models.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace sales {
class SettingsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "sales-settings";
    SettingsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    models::SalesSettings GetSettings(const boost::uuids::uuid& organization) const;
    models::SalesSettings UpdateSettings(const models::UpdateSalesSettingsRequest& request) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace sales

template <>
inline constexpr bool userver::components::kHasValidate<sales::SettingsComponent> = true;
