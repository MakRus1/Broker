#pragma once
#include <openapi/models.hpp>
#include <userver/formats/json.hpp>
#include <userver/formats/json/exception.hpp>
#include <userver/formats/parse/common_containers.hpp>

namespace web_bff::converters {
inline std::vector<models::City> ConvertCitySuggestions(
    const userver::formats::json::Value& document) {
    const auto& items = document["suggestions"];
    if (!items.IsArray() || items.GetSize() > 10)
        throw userver::formats::json::ConversionException{
            "Некорректный ответ поиска городов", items.GetPath()};
    std::vector<models::City> result;
    for (const auto& item : items) {
        const auto& data = item["data"];
        const auto name = data["city"].As<std::optional<std::string>>();
        const auto code = data["country_iso_code"].As<std::optional<std::string>>();
        if (!name.has_value() || !code.has_value() || code.value().size() != 2)
            continue;
        result.push_back({.country = data["country"].As<std::string>(),
            .country_code = code.value(),
            .label = item["value"].As<std::string>(),
            .name = name.value()});
    }
    return result;
}
}  // namespace web_bff::converters
