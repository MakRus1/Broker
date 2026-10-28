#pragma once
#include <cstdint>

namespace shipments::helpers {
struct ContainerTotals {
    std::int64_t Created{0};
    std::int64_t Pending{0};
};
}  // namespace shipments::helpers
