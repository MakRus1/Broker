#pragma once
#include <helpers/catalog.hpp>
#include <regex>
#include <set>
#include <tuple>
#include <userver/formats/json.hpp>
#include <userver/storages/postgres/io/row_types.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace localization::helpers {
namespace json = userver::formats::json;
using Catalog = models::EditableCatalog;

inline std::set<std::string> Parameters(const std::string& text) {
    static const std::regex pattern{R"(\{\w+\})"};
    std::set<std::string> result;
    for (auto it = std::sregex_iterator(text.begin(), text.end(), pattern);
         it != std::sregex_iterator(); ++it)
        result.insert(it->str());
    return result;
}

inline json::Value EncodeCatalog(const Catalog& catalog, const json::Value& source) {
    const auto ru = source["ru"];
    const auto en = source["en"];
    json::ValueBuilder result{json::Type::kObject};
    const auto encode = [&](const auto& entries, const auto& builtin, const char* locale) {
        result[locale] = json::ValueBuilder{json::Type::kObject};
        std::set<std::string> keys;
        for (const auto& entry : entries) {
            if (!builtin.HasMember(entry.key) || !keys.insert(entry.key).second ||
                entry.value.empty() || entry.value.size() > 10000)
                throw ApiError<400>{"invalid_translation", "Некорректный перевод: " + entry.key};
            if (Parameters(entry.value) !=
                Parameters(builtin[entry.key].template As<std::string>()))
                throw ApiError<400>{
                    "invalid_parameters", "Не совпадают параметры перевода: " + entry.key};
            result[locale][entry.key] = entry.value;
        }
    };
    encode(catalog.ru, ru, "ru");
    encode(catalog.en, en, "en");
    auto value = result.ExtractValue();
    if (json::ToString(value).size() > 2 * 1024 * 1024)
        throw ApiError<400>{"catalog_too_large", "Каталог превышает 2 МБ"};
    return value;
}

inline Catalog DecodeCatalog(const json::Value& value) {
    Catalog result;
    const auto decode = [&](const char* locale, auto& entries) {
        if (!value.HasMember(locale))
            return;
        for (auto it = value[locale].begin(); it != value[locale].end(); ++it)
            entries.push_back({.key = it.GetName(), .value = it->template As<std::string>()});
    };
    decode("ru", result.ru);
    decode("en", result.en);
    return result;
}

// Called inside the writer transaction too: returned version and catalogs are one snapshot.
inline models::CatalogEditorState ReadEditor(pg::Transaction& tx, const std::string& scope) {
    models::CatalogEditorState result;
    result.source = DecodeCatalog(tx.Execute(sql::kGetSourceCatalog).AsSingleRow<json::Value>());
    result.version = 0;
    result.revision = 0;
    const auto state = tx.Execute(sql::kGetEditor, scope);
    if (!state.IsEmpty()) {
        auto [version, revision, draft] =
            state.AsSingleRow<std::tuple<std::int64_t, std::int64_t, std::optional<json::Value>>>(
                pg::kRowTag);
        result.version = version;
        result.revision = revision;
        if (draft)
            result.draft = DecodeCatalog(*draft);
    }
    const auto [published, inherited] =
        tx.Execute(sql::kGetEditorCatalogs, scope)
            .AsSingleRow<std::tuple<json::Value, json::Value>>(pg::kRowTag);
    result.published = DecodeCatalog(published);
    result.inherited = DecodeCatalog(inherited);
    return result;
}

inline models::CatalogEditorState GetEditor(const Deps& deps, const std::string& scope) {
    auto tx = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster,
        pg::TransactionOptions{
            pg::IsolationLevel::kRepeatableRead, pg::TransactionOptions::kReadOnly});
    auto result = ReadEditor(tx, scope);
    tx.Commit();
    return result;
}

inline models::CatalogEditorState ChangeEditor(const Deps& deps, const std::string& scope,
    std::int64_t expected, const std::optional<Catalog>& catalog) {
    // Регистрация исходных текстов не меняет черновики; публикация проверяет их заново.
    if (scope != "global" && catalog) {
        for (const auto* entries : {&catalog->ru, &catalog->en})
            for (const auto& entry : *entries)
                if (entry.key.starts_with("login.") || entry.key.starts_with("keycloak."))
                    throw ApiError<400>{"global_translation_only",
                        "Переводы входа доступны только в общем каталоге"};
    }
    auto tx = deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    tx.Execute(sql::kEnsureEditor, scope);
    const auto [version, revision, draft] =
        tx.Execute(sql::kLockEditor, scope)
            .AsSingleRow<std::tuple<std::int64_t, std::int64_t, std::optional<json::Value>>>(
                pg::kRowTag);
    if (version != expected)
        throw ApiError<409>{
            "catalog_changed", "Переводы изменены. Обновите редактор перед сохранением"};
    const auto source = tx.Execute(sql::kGetSourceCatalog).AsSingleRow<json::Value>();
    if (catalog) {
        const auto encoded = EncodeCatalog(*catalog, source);
        tx.Execute(sql::kSaveEditor, scope, encoded);
    } else {
        if (!draft)
            throw ApiError<409>{"draft_required", "Нет черновика для публикации"};
        const auto valid = EncodeCatalog(DecodeCatalog(*draft), source);
        for (const auto& locale : {"ru", "en"}) {
            if (scope == "global")
                tx.Execute(sql::kPublishCatalog, locale, valid[locale], revision + 1);
            else
                tx.Execute(
                    sql::kPublishOrganizationCatalog, scope, locale, valid[locale], revision + 1);
        }
        tx.Execute(sql::kPublishEditor, scope);
    }
    auto result = ReadEditor(tx, scope);
    tx.Commit();
    return result;
}
}  // namespace localization::helpers
