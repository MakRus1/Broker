#pragma once
#include <boost/uuid/uuid.hpp>
#include <string>
#include <vector>

namespace organizations::models {
struct AccessRow {
    boost::uuids::uuid Organization;
    std::vector<std::string> Permissions;
};
}  // namespace organizations::models
