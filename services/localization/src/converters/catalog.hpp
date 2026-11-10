#pragma once
#include <openapi/models.hpp>
#include <userver/formats/json.hpp>

namespace localization::converters {
namespace json = userver::formats::json;

inline models::EditableCatalog DecodeCatalog(const json::Value& value) {
    models::EditableCatalog result;
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

inline json::Value EncodeCatalog(const models::EditableCatalog& catalog) {
    json::ValueBuilder result{json::Type::kObject};
    const auto encode = [&](const auto& entries, const char* locale) {
        result[locale] = json::ValueBuilder{json::Type::kObject};
        for (const auto& entry : entries)
            result[locale][entry.key] = entry.value;
    };
    encode(catalog.ru, "ru");
    encode(catalog.en, "en");
    return result.ExtractValue();
}

inline std::vector<models::Translation> ConvertMessages(const json::Value& messages) {
    std::vector<models::Translation> result;
    for (auto it = messages.begin(); it != messages.end(); ++it)
        if (it->IsString())
            result.push_back({.key = it.GetName(), .value = it->As<std::string>()});
    return result;
}

struct PublicTranslationRow {
    std::string Key;
    json::Value Value;
};

inline models::Translation Convert(const PublicTranslationRow& row) {
    return {.key = row.Key, .value = row.Value.As<std::string>()};
}
}  // namespace localization::converters
