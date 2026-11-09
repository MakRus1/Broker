#pragma once
#include <optional>
#include <string>
#include <userver/utils/text_light.hpp>

namespace counterparties::helpers {
inline std::optional<std::string> Normalize(std::optional<std::string> value) {
    if (!value.has_value())
        return std::nullopt;
    value = userver::utils::text::Trim(value.value());
    return value.value().empty() ? std::nullopt : value;
}
}  // namespace counterparties::helpers
