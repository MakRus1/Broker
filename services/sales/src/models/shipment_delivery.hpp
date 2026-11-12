#pragma once
#include <optional>
#include <string>

namespace sales::models {
struct ShipmentDelivery {
    std::optional<std::string> ShipmentId;
    std::optional<std::string> Rejection;
};

}  // namespace sales::models
