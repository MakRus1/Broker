#pragma once
#include <cstdint>
#include <userver/utils/datetime/date.hpp>

namespace sales::models {
struct NumberAllocation {
    userver::utils::datetime::Date Date;
    std::int64_t Sequence;
};
}  // namespace sales::models
