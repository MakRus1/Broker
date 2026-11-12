#pragma once
#include <openapi/models.hpp>

namespace transportations::route_rules {
enum class ActualChange { Record, Correction };
std::size_t ApplyActual(models::Transportation&, const std::string&, models::EventKind,
    std::chrono::system_clock::time_point, ActualChange);
}  // namespace transportations::route_rules
