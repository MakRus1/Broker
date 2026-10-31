#pragma once
#include <cstdint>
#include <platform/calendar_date.hpp>
#include <platform/handler_error.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace platform {
inline bool IsDocumentNumberTemplate(const std::string& value, std::size_t maximum = 128) {
    if (value.empty() || value.size() > maximum || value.find("{sequence}") == std::string::npos)
        return false;
    auto rest = value;
    for (const auto token : {"{sequence}", "{yyyy}", "{yy}", "{mm}", "{dd}"}) {
        std::size_t at;
        while ((at = rest.find(token)) != std::string::npos)
            rest.erase(at, std::string_view{token}.size());
    }
    return rest.find_first_of("{}") == std::string::npos;
}

inline void ValidateNumberTemplate(const std::string& value, std::size_t maximum = 128) {
    if (!IsDocumentNumberTemplate(value, maximum))
        throw http::HandlerError<400>{"invalid_number_template",
            "Укажите шаблон с {sequence}; доступны {yyyy}, {yy}, {mm}, {dd}"};
}

inline int DocumentNumberPeriod(userver::utils::datetime::Date date, bool yearly) {
    return yearly ? static_cast<int>(std::chrono::year_month_day{date.GetSysDays()}.year()) : 0;
}

inline std::string FormatDocumentNumber(
    std::string mask, userver::utils::datetime::Date date, std::int64_t sequence) {
    ValidateNumberTemplate(mask);
    const auto day = FormatCalendarDate(date);
    for (const auto& [token, value] : std::vector<std::pair<std::string, std::string>>{
             {"{sequence}", std::to_string(sequence)}, {"{yyyy}", day.substr(0, 4)},
             {"{yy}", day.substr(2, 2)}, {"{mm}", day.substr(5, 2)}, {"{dd}", day.substr(8, 2)}}) {
        std::size_t at = 0;
        while ((at = mask.find(token, at)) != std::string::npos) {
            mask.replace(at, token.size(), value);
            at += value.size();
        }
    }
    return mask;
}
}  // namespace platform
