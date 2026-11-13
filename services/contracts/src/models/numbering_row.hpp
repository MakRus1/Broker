#pragma once
#include <cstdint>
#include <string>

namespace contracts::models {
struct NumberingRow {
    std::string NumberTemplate;
    bool ResetYearly;
    std::int64_t Revision;
};

}  // namespace contracts::models
