#pragma once

#include <models/json.hpp>
#include <string>

namespace configs {
struct Revision {
    std::int64_t Id;
    Json Values;
};
struct RevisionInfo {
    std::int64_t Id;
    std::string CreatedAt;
};
}  // namespace configs
