#pragma once
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace localization {
class SourcesComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "localization-sources";
    SourcesComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    int Register(const models::EditableCatalog&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace localization

template <>
inline constexpr bool userver::components::kHasValidate<localization::SourcesComponent> = true;
