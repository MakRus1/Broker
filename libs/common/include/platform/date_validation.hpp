#pragma once
#include <chrono>
#include <userver/utils/datetime/date.hpp>

namespace platform {
// Календарная дата разобрана на границе API: здесь проверяется бизнесовый диапазон.
inline bool IsCalendarDateInRange(userver::utils::datetime::Date value,
    std::chrono::year_month_day minimum, std::chrono::year_month_day maximum) {
    return value.GetSysDays() >= std::chrono::sys_days{minimum} &&
        value.GetSysDays() <= std::chrono::sys_days{maximum};
}
}  // namespace platform
