#pragma once
#include <cstdint>
#include <string>
#include <userver/utils/datetime/date.hpp>

namespace finance::models {
struct ConversionRate {
    std::int64_t Millionths;
    userver::utils::datetime::Date Date;
    std::string Source;
};
}  // namespace finance::models
