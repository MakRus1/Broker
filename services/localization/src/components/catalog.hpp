#pragma once
#include <clients/organizations/client.hpp>
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace localization {
class CatalogComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "localization-catalog";
    CatalogComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    std::vector<models::Translation> Get(
        const models::GetCatalogRequest&, const userver::server::request::RequestContext&) const;
    std::vector<models::Translation> GetPublic(const std::string& locale) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
    void CheckOrganization(
        const userver::server::request::RequestContext&, const std::string&) const;
    clients::organizations::Client organizations_;
};
}  // namespace localization

template <>
inline constexpr bool userver::components::kHasValidate<localization::CatalogComponent> = true;
