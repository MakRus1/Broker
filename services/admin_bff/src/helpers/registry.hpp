#pragma once
#include <optional>

namespace admin_bff {
template <typename Target, typename Source>
std::optional<Target> ConvertRegistryCursor(const std::optional<Source>& value) {
    if (!value.has_value())
        return std::nullopt;
    return Target{
        .id = value.value().id, .scope = value.value().scope, .value = value.value().value};
}
}  // namespace admin_bff
