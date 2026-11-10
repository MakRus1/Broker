#pragma once
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace localization {
class EditorComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "localization-editor";
    EditorComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::CatalogEditorState Get(const models::GetEditorRequest&) const;
    models::CatalogEditorState Save(const models::SaveCatalogRequest&) const;
    models::CatalogEditorState Publish(const models::PublishCatalogRequest&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
    models::CatalogEditorState Change(
        const std::string&, std::int64_t, const std::optional<models::EditableCatalog>&) const;
};
}  // namespace localization

template <>
inline constexpr bool userver::components::kHasValidate<localization::EditorComponent> = true;
