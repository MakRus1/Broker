#pragma once
#include <helpers/session.hpp>

namespace transportations::helpers {
enum class ActualChange { Record, Correction };

// Общая для записи и исправления часть; транзакция и аудит остаются видны в ручке.
void UpdateActual(Session&, models::Transportation&, const std::string& leg_id, models::EventKind,
    const std::string& occurred_at, ActualChange);
}  // namespace transportations::helpers
