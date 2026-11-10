#include <components/editor.hpp>
#include <converters/catalog.hpp>
#include <helpers/parameters.hpp>
#include <localization/sql_queries.hpp>
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
using Catalog = models::EditableCatalog;
using helpers::Parameters;

namespace {
struct EditorRow {
    std::int64_t Version;
    std::int64_t Revision;
    std::optional<json::Value> Draft;
};

struct EditorCatalogsRow {
    json::Value Published;
    json::Value Inherited;
};

json::Value ValidateAndEncodeCatalog(const Catalog& catalog, const json::Value& source) {
    const auto ru = source["ru"];
    const auto en = source["en"];
    const auto validate = [&](const auto& entries, const auto& builtin) {
        std::set<std::string> keys;
        for (const auto& entry : entries) {
            if (!builtin.HasMember(entry.key) || !keys.insert(entry.key).second ||
                entry.value.empty() || entry.value.size() > 10000)
                throw ApiError<400>{"invalid_translation", "Некорректный перевод: " + entry.key};
            if (Parameters(entry.value) !=
                Parameters(builtin[entry.key].template As<std::string>()))
                throw ApiError<400>{
                    "invalid_parameters", "Не совпадают параметры перевода: " + entry.key};
        }
    };
    validate(catalog.ru, ru);
    validate(catalog.en, en);
    auto value = converters::EncodeCatalog(catalog);
    if (json::ToString(value).size() > 2 * 1024 * 1024)
        throw ApiError<400>{"catalog_too_large", "Каталог превышает 2 МБ"};
    return value;
}

// В транзакции записи версия и каталоги читаются из одного снимка.
models::CatalogEditorState ReadEditor(pg::Transaction& tx, const std::string& scope) {
    models::CatalogEditorState result;
    result.source =
        converters::DecodeCatalog(tx.Execute(sql::kGetSourceCatalog).AsSingleRow<json::Value>());
    result.version = 0;
    result.revision = 0;
    const auto state = tx.Execute(sql::kGetEditor, scope);
    if (!state.IsEmpty()) {
        const auto row = state.AsSingleRow<EditorRow>(pg::kRowTag);
        result.version = row.Version;
        result.revision = row.Revision;
        if (row.Draft.has_value())
            result.draft = converters::DecodeCatalog(row.Draft.value());
    }
    const auto catalogs =
        tx.Execute(sql::kGetEditorCatalogs, scope).AsSingleRow<EditorCatalogsRow>(pg::kRowTag);
    result.published = converters::DecodeCatalog(catalogs.Published);
    result.inherited = converters::DecodeCatalog(catalogs.Inherited);
    return result;
}

}  // namespace

EditorComponent::EditorComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(context.FindComponent<userver::components::Postgres>("postgres-main").GetCluster()) {
}

userver::yaml_config::Schema EditorComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::CatalogEditorState EditorComponent::Get(const models::GetEditorRequest& request) const {
    const auto scope = request.organization_id.value_or("global");
    auto tx = cluster_->Begin(pg::ClusterHostType::kMaster,
        pg::TransactionOptions{
            pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
    auto result = ReadEditor(tx, scope);
    tx.Commit();
    return result;
}

models::CatalogEditorState EditorComponent::Change(
    const std::string& scope, std::int64_t expected, const std::optional<Catalog>& catalog) const {
    // Регистрация исходных текстов не меняет черновики; публикация проверяет их заново.
    if (scope != "global" && catalog.has_value()) {
        for (const auto* entries : {&catalog.value().ru, &catalog.value().en})
            for (const auto& entry : *entries)
                if (entry.key.starts_with("login.") || entry.key.starts_with("keycloak."))
                    throw ApiError<400>{"global_translation_only",
                        "Переводы входа доступны только в общем каталоге"};
    }
    auto tx = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    tx.Execute(sql::kEnsureEditor, scope);
    const auto row = tx.Execute(sql::kLockEditor, scope).AsSingleRow<EditorRow>(pg::kRowTag);
    if (row.Version != expected)
        throw ApiError<409>{
            "catalog_changed", "Переводы изменены. Обновите редактор перед сохранением"};
    const auto source = tx.Execute(sql::kGetSourceCatalog).AsSingleRow<json::Value>();
    if (catalog.has_value()) {
        const auto encoded = ValidateAndEncodeCatalog(catalog.value(), source);
        tx.Execute(sql::kSaveEditor, scope, encoded);
    } else {
        if (!row.Draft.has_value())
            throw ApiError<409>{"draft_required", "Нет черновика для публикации"};
        const auto valid =
            ValidateAndEncodeCatalog(converters::DecodeCatalog(row.Draft.value()), source);
        for (const auto& locale : {"ru", "en"}) {
            if (scope == "global")
                tx.Execute(sql::kPublishCatalog, locale, valid[locale], row.Revision + 1);
            else
                tx.Execute(sql::kPublishOrganizationCatalog, scope, locale, valid[locale],
                    row.Revision + 1);
        }
        tx.Execute(sql::kPublishEditor, scope);
    }
    auto result = ReadEditor(tx, scope);
    tx.Commit();
    return result;
}

models::CatalogEditorState EditorComponent::Save(const models::SaveCatalogRequest& request) const {
    return Change(request.organization_id.value_or("global"), request.version, request.catalog);
}

models::CatalogEditorState EditorComponent::Publish(
    const models::PublishCatalogRequest& request) const {
    return Change(request.organization_id.value_or("global"), request.version, std::nullopt);
}
}  // namespace localization
