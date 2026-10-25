#pragma once

#include <chrono>
#include <stdexcept>
#include <string>
#include <userver/utils/datetime/from_string_saturating.hpp>

namespace platform {
inline std::chrono::system_clock::time_point ParseTimestamp(std::string text) {
    // Bound the year before any duration conversion. Microseconds can represent all four-digit
    // years.
    if (text.size() < 20 || text[4] != '-' || text.substr(0, 4) == "0000")
        throw std::invalid_argument("date-time");
    for (std::size_t index = 0; index < 4; ++index)
        if (text[index] < '0' || text[index] > '9')
            throw std::invalid_argument("date-time");
    if (text[10] == 't')
        text[10] = 'T';
    if (text.back() == 'z')
        text.back() = 'Z';
    using Clock = std::chrono::system_clock;
    const auto coarse =
        userver::utils::datetime::FromRfc3339StringSaturating<std::chrono::microseconds>(text);
    // Reject values outside the native clock before converting to nanoseconds.
    // Reserve the last partial microsecond to avoid overflow from a fractional second.
    if (coarse < std::chrono::ceil<std::chrono::microseconds>(Clock::time_point::min()) ||
        coarse >= std::chrono::floor<std::chrono::microseconds>(Clock::time_point::max()))
        throw std::invalid_argument("date-time outside system_clock range");
    return userver::utils::datetime::FromRfc3339StringSaturating<Clock::duration>(text);
}
}  // namespace platform
