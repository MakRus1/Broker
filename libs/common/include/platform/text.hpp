#pragma once
#include <optional>
#include <string>
#include <userver/utils/text_light.hpp>
#include <utility>

namespace platform {
inline std::optional<std::string> TrimToOptional(const std::optional<std::string>& value) {
    if (!value.has_value())
        return std::nullopt;
    auto text = userver::utils::text::Trim(value.value());
    return text.empty() ? std::nullopt : std::optional<std::string>{std::move(text)};
}
}  // namespace platform
