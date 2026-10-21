#pragma once
#include <cstdint>
#include <models/errors.hpp>

namespace shipments::helpers {
inline void CheckRevision(std::int64_t actual, std::int64_t expected) {
    if (actual != expected)
        throw ApiError{409, "revision_conflict", "Объект изменен; перечитайте актуальную редакцию"};
}
}  // namespace shipments::helpers
