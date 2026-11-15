#pragma once
#include <boost/uuid/uuid.hpp>
#include <cstdint>
#include <optional>
#include <string>

namespace notifications {
struct NotificationDelivery {
    std::string RecipientIssuer;
    std::optional<std::string> RecipientSubject;
    boost::uuids::uuid EntityId;
    std::string EntityNumber;
    std::int64_t Revision;
};
}  // namespace notifications
