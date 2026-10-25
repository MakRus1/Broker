#pragma once

#include <chrono>
#include <fmt/format.h>
#include <stdexcept>
#include <string>
#include <userver/utils/datetime/date.hpp>
#include <userver/utils/datetime/from_string_saturating.hpp>

namespace platform {
// Calendar days must not pass through system_clock's nanosecond duration (1677–2262).
inline userver::utils::datetime::Date ParseCalendarDate(const std::string& text) {
    if (text.substr(0, 4) == "0000")
        throw std::invalid_argument("date");
    const auto seconds =
        userver::utils::datetime::FromStringSaturating<std::chrono::seconds>(text, "%Y-%m-%d");
    const userver::utils::datetime::Date date{std::chrono::floor<std::chrono::days>(seconds)};
    if (date < userver::utils::datetime::Date{1, 1, 1} ||
        date > userver::utils::datetime::Date{9999, 12, 31})
        throw std::invalid_argument("date");
    return date;
}

inline std::string FormatCalendarDate(userver::utils::datetime::Date date) {
    if (date < userver::utils::datetime::Date{1, 1, 1} ||
        date > userver::utils::datetime::Date{9999, 12, 31})
        throw std::invalid_argument("date");
    const std::chrono::year_month_day calendar{date.GetSysDays()};
    return fmt::format("{:04}-{:02}-{:02}", static_cast<int>(calendar.year()),
        static_cast<unsigned>(calendar.month()), static_cast<unsigned>(calendar.day()));
}
}  // namespace platform
