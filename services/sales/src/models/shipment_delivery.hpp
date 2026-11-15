#pragma once
#include <boost/uuid/uuid.hpp>
#include <optional>
#include <string>

namespace sales::models {
struct ShipmentDelivery {
    std::optional<boost::uuids::uuid> ShipmentId;
    std::optional<std::string> Rejection;
};

}  // namespace sales::models
