#include <components/editor.hpp>
#include <converters/catalog.hpp>
#include <helpers/catalog_validation.hpp>
#include <localization/sql_queries.hpp>
#include <models/editor_catalogs_row.hpp>
#include <models/editor_row.hpp>
#include <platform/handler_error.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/transaction.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace localization {
namespace pg = userver::storages::postgres;
namespace json = userver::formats::json;
template <int Status> using ApiError = platform::http::HandlerError<Status>;

// В транзакции записи версия и каталоги читаются из одного снимка.
models::CatalogEditorState EditorComponent::ReadEditorSnapshot(
    pg::Transaction& tx, const std::string& scope) const {
    models::CatalogEditorState result;
    result.source =
        converters::DecodeCatalog(tx.Execute(sql::kGetSourceCatalog).AsSingleRow<json::Value>());
    result.version = 0;
    result.revision = 0;
    const auto state = tx.Execute(sql::kGetEditor, scope);
    if (!state.IsEmpty()) {
        const auto row = state.AsSingleRow<models::EditorRow>(pg::kRowTag);
        result.version = row.Version;
        result.revision = row.Revision;
        if (row.Draft.has_value())
            result.draft = converters::DecodeCatalog(row.Draft.value());
    }
    const auto catalogs = tx.Execute(sql::kGetEditorCatalogs, scope)
                              .AsSingleRow<models::EditorCatalogsRow>(pg::kRowTag);
    result.published = converters::DecodeCatalog(catalogs.Published);
    result.inherited = converters::DecodeCatalog(catalogs.Inherited);
    return result;
}

EditorComponent::EditorComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-main").GetCluster()) {
}

userver::yaml_config::Schema EditorComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::CatalogEditorState EditorComponent::GetEditorState(
    const models::GetEditorRequest& request) const {
    const auto scope = request.organization_id.value_or("global");
    auto tx = cluster_->Begin(pg::ClusterHostType::kMaster,
        pg::TransactionOptions{
            pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
    auto result = ReadEditorSnapshot(tx, scope);
    tx.Commit();
    return result;
}

models::CatalogEditorState EditorComponent::SaveDraft(
    const models::SaveCatalogRequest& request) const {
    const auto scope = request.organization_id.value_or("global");
    const auto& catalog = request.catalog;
    if (scope != "global") {
        for (const auto* entries : {&catalog.ru, &catalog.en})
            for (const auto& entry : *entries)
                if (entry.key.starts_with("login.") || entry.key.starts_with("keycloak."))
                    throw ApiError<400>{"global_translation_only",
                        "Переводы входа доступны только в общем каталоге"};
    }

    auto tx = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    tx.Execute(sql::kEnsureEditor, scope);
    const auto row =
        tx.Execute(sql::kLockEditor, scope).AsSingleRow<models::EditorRow>(pg::kRowTag);
    if (row.Version != request.version)
        throw ApiError<409>{
            "catalog_changed", "Переводы изменены. Обновите редактор перед сохранением"};

    const auto source = tx.Execute(sql::kGetSourceCatalog).AsSingleRow<json::Value>();
    const auto encoded = helpers::ValidateAndEncodeCatalog(catalog, source);
    tx.Execute(sql::kSaveEditor, scope, encoded);

    auto result = ReadEditorSnapshot(tx, scope);
    tx.Commit();
    return result;
}

models::CatalogEditorState EditorComponent::PublishDraft(
    const models::PublishCatalogRequest& request) const {
    const auto scope = request.organization_id.value_or("global");
    auto tx = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    tx.Execute(sql::kEnsureEditor, scope);
    const auto row =
        tx.Execute(sql::kLockEditor, scope).AsSingleRow<models::EditorRow>(pg::kRowTag);
    if (row.Version != request.version)
        throw ApiError<409>{
            "catalog_changed", "Переводы изменены. Обновите редактор перед сохранением"};
    if (!row.Draft.has_value())
        throw ApiError<409>{"draft_required", "Нет черновика для публикации"};

    // Исходные тексты могли обновиться после сохранения черновика.
    const auto source = tx.Execute(sql::kGetSourceCatalog).AsSingleRow<json::Value>();
    const auto valid =
        helpers::ValidateAndEncodeCatalog(converters::DecodeCatalog(row.Draft.value()), source);
    for (const auto& locale : {"ru", "en"}) {
        if (scope == "global")
            tx.Execute(sql::kPublishCatalog, locale, valid[locale], row.Revision + 1);
        else
            tx.Execute(
                sql::kPublishOrganizationCatalog, scope, locale, valid[locale], row.Revision + 1);
    }
    tx.Execute(sql::kPublishEditor, scope);

    auto result = ReadEditorSnapshot(tx, scope);
    tx.Commit();
    return result;
}
}  // namespace localization
