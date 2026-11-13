#pragma once
#include <string>
#include <vector>

namespace organizations::models {
struct AccessRow {
    std::string Organization;
    std::vector<std::string> Permissions;
};
}  // namespace organizations::models
