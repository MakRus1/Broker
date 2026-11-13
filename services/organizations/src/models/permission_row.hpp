#pragma once
#include <string>
#include <vector>

namespace organizations::models {
struct PermissionRow {
    std::string Group;
    std::string Id;
    std::string Name;
    std::vector<std::string> Requires;
};
}  // namespace organizations::models
