#pragma once
#include <boost/uuid/uuid.hpp>
#include <optional>
#include <string>

namespace shipments {
struct ShipmentSource {
    std::string Producer;
    std::string Type;
    std::optional<boost::uuids::uuid> Id;
    std::optional<std::string> ManualIdempotencyKey;

    static ShipmentSource Manual(const std::string& key) {
        // Источник описывает пользовательский сценарий, а не приложение или окружение.
        return {.Producer = "user", .Type = "manual", .ManualIdempotencyKey = key};
    }
};

}  // namespace shipments
