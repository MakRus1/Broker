#pragma once
#include <chrono>
#include <optional>
#include <string_view>

namespace platform {
// Строгое ISO-представление календарной даты, без времени и часового пояса.
inline std::optional<std::chrono::year_month_day> ParseCalendarDate(std::string_view value) {
    if (value.size() != 10 || value[4] != '-' || value[7] != '-')
        return std::nullopt;
    int year = 0, month = 0, day = 0;
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (index == 4 || index == 7)
            continue;
        if (value[index] < '0' || value[index] > '9')
            return std::nullopt;
        auto& part = index < 4 ? year : index < 7 ? month : day;
        part = part * 10 + value[index] - '0';
    }
    const std::chrono::year_month_day date{std::chrono::year{year},
        std::chrono::month{static_cast<unsigned>(month)},
        std::chrono::day{static_cast<unsigned>(day)}};
    if (year == 0 || !date.ok())
        return std::nullopt;
    return date;
}

inline bool IsCalendarDateInRange(std::string_view value, std::chrono::year_month_day minimum,
    std::chrono::year_month_day maximum) {
    const auto date = ParseCalendarDate(value);
    return date.has_value() && date.value() >= minimum && date.value() <= maximum;
}
}  // namespace platform
