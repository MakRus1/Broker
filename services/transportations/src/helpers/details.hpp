#pragma once
#include <models/errors.hpp>
#include <openapi/models.hpp>
#include <userver/utils/text.hpp>

namespace transportations::helpers {
inline void ValidateDetails(models::TransportationType type,
    const std::optional<std::string>& container_type,
    const std::optional<models::ContainerOwnership>& ownership,
    const std::optional<std::string>& container_number) {
    if (type != models::TransportationType::Container &&
        (container_type.has_value() || ownership.has_value() || container_number.has_value()))
        throw ApiError{400, "unexpected_container",
            "Контейнерные поля доступны только для контейнерной перевозки"};
}

inline std::optional<std::string> CleanContainerNumber(const std::optional<std::string>& value) {
    if (!value.has_value())
        return std::nullopt;
    auto result = userver::utils::text::Trim(value.value());
    if (result.empty())
        return std::nullopt;
    return result;
}

inline std::optional<std::string> Ownership(
    const std::optional<models::ContainerOwnership>& value) {
    if (!value.has_value())
        return std::nullopt;
    return std::string{models::EnumValue(value.value())};
}
}  // namespace transportations::helpers
