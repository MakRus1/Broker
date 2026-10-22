#pragma once
#include <stdexcept>
#include <string>
#include <string_view>
#include <userver/utils/trivial_map.hpp>

namespace shipments::helpers {
enum class ShipmentDictionary { kIncoterms, kCarriageTerms, kLoading, kRouteType };

inline constexpr userver::utils::TrivialBiMap kShipmentDictionaries = [](auto selector) {
    return selector()
        .Case("incoterms", ShipmentDictionary::kIncoterms)
        .Case("carriage_terms", ShipmentDictionary::kCarriageTerms)
        .Case("loading", ShipmentDictionary::kLoading)
        .Case("route_type", ShipmentDictionary::kRouteType);
};

inline ShipmentDictionary ParseDictionary(std::string_view value) {
    const auto dictionary = kShipmentDictionaries.TryFindByFirst(value);
    if (!dictionary.has_value())
        throw std::runtime_error("Неизвестный справочник поставок: " + std::string{value});
    return dictionary.value();
}

inline std::string_view DictionaryCode(ShipmentDictionary value) {
    const auto code = kShipmentDictionaries.TryFindBySecond(value);
    if (!code.has_value())
        throw std::logic_error("Некорректный вид справочника поставок");
    return code.value();
}
}  // namespace shipments::helpers
