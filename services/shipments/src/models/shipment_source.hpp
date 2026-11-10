#pragma once
#include <string>

namespace shipments {
struct ShipmentSource {
    std::string Producer;
    std::string Type;
    std::string Id;

    static ShipmentSource Manual(const std::string& key) {
        // Источник описывает пользовательский сценарий, а не приложение или окружение.
        return {.Producer = "user", .Type = "manual", .Id = key};
    }
};

}  // namespace shipments
