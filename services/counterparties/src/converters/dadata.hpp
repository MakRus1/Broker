#pragma once
#include <openapi/models.hpp>
#include <userver/formats/json.hpp>
#include <userver/formats/json/exception.hpp>
#include <userver/formats/parse/common_containers.hpp>

namespace counterparties::converters {
inline models::RegistryParty Convert(const userver::formats::json::Value& item) {
    const auto& data = item["data"];
    return {.kpp = data["kpp"].As<std::optional<std::string>>(),
        .legal_address = data["address"].IsNull() || data["address"].IsMissing()
            ? std::nullopt
            : data["address"]["unrestricted_value"].As<std::optional<std::string>>(),
        .legal_name = data["name"]["full_with_opf"].As<std::optional<std::string>>(),
        .name = data["name"]["short_with_opf"].As<std::optional<std::string>>().value_or(
            item["value"].As<std::string>()),
        .registration_id = data["ogrn"].As<std::optional<std::string>>(),
        .status = data["state"]["status"].As<std::optional<std::string>>(),
        .tax_id = data["inn"].As<std::string>()};
}

inline std::vector<models::RegistryParty> ConvertDaDataResponse(
    const userver::formats::json::Value& document, const std::string& tax_id) {
    const auto& items = document["suggestions"];
    if (!items.IsArray() || items.GetSize() > 1)
        throw userver::formats::json::ConversionException{
            "Ожидается не больше одного результата DaData", "suggestions"};

    std::vector<models::RegistryParty> result;
    for (const auto& item : items) {
        auto value = Convert(item);
        if (value.tax_id != tax_id || value.name.empty())
            throw userver::formats::json::ConversionException{
                "Ответ DaData не соответствует запросу", item.GetPath()};
        result.push_back(std::move(value));
    }
    return result;
}
}  // namespace counterparties::converters
