#pragma once
#include <clients/localization/client.hpp>
#include <openapi/models.hpp>

namespace admin_bff::converters {
inline models::EditableCatalog Convert(
    const clients::localization::models::EditableCatalog& value) {
    models::EditableCatalog result;
    result.en.reserve(value.en.size());
    result.ru.reserve(value.ru.size());
    for (const auto& item : value.en)
        result.en.push_back({.key = item.key, .value = item.value});
    for (const auto& item : value.ru)
        result.ru.push_back({.key = item.key, .value = item.value});
    return result;
}

inline clients::localization::models::EditableCatalog Convert(
    const models::EditableCatalog& value) {
    clients::localization::models::EditableCatalog result;
    result.en.reserve(value.en.size());
    result.ru.reserve(value.ru.size());
    for (const auto& item : value.en)
        result.en.push_back({.key = item.key, .value = item.value});
    for (const auto& item : value.ru)
        result.ru.push_back({.key = item.key, .value = item.value});
    return result;
}

template <typename Result, typename Source> Result Convert(const Source& value) {
    return {.draft = value.draft ? std::optional{Convert(*value.draft)} : std::nullopt,
        .inherited = Convert(value.inherited),
        .published = Convert(value.published),
        .revision = value.revision,
        .source = Convert(value.source),
        .version = value.version};
}
}  // namespace admin_bff::converters
