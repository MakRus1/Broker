#pragma once
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace localization {
class EditorComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "localization-editor";
    EditorComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::CatalogEditorState GetEditorState(const models::GetEditorRequest&) const;
    models::CatalogEditorState SaveDraft(const models::SaveCatalogRequest&) const;
    models::CatalogEditorState PublishDraft(const models::PublishCatalogRequest&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
    models::CatalogEditorState ReadEditorSnapshot(
        userver::storages::postgres::Transaction&, const std::string& scope) const;
};
}  // namespace localization

template <>
inline constexpr bool userver::components::kHasValidate<localization::EditorComponent> = true;
