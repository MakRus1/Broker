#pragma once
#include <cstdint>
#include <optional>
#include <string>

namespace notifications {
struct NotificationDelivery {
    std::string RecipientIssuer;
    std::optional<std::string> RecipientSubject;
    std::string EntityId;
    std::string EntityNumber;
    std::int64_t Revision;
};
}  // namespace notifications
