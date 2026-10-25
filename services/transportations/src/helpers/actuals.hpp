#pragma once
#include <helpers/database.hpp>
#include <openapi/models.hpp>

namespace transportations::helpers {
enum class ActualChange { Record, Correction };

std::size_t ApplyActual(models::Transportation&, const std::string& route_segment_id,
    models::EventKind, std::chrono::system_clock::time_point occurred_at, ActualChange);

// Общая для записи и исправления часть; транзакция и аудит остаются видны в ручке.
void UpdateActual(pg::Transaction&, const std::string& organization, models::Transportation&,
    const std::string& route_segment_id, models::EventKind,
    std::chrono::system_clock::time_point occurred_at, ActualChange);
}  // namespace transportations::helpers
