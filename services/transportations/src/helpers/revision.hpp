#pragma once
#include <cstdint>
#include <models/errors.hpp>

namespace transportations::helpers {
inline void CheckRevision(std::int64_t actual, std::int64_t expected) {
    if (actual != expected)
        throw RevisionConflict{};
}
}  // namespace transportations::helpers
