#pragma once
#include <openapi/models.hpp>
#include <userver/formats/json.hpp>
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
}  // namespace counterparties::converters
