#pragma once
#include <helpers/database.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace transportations::helpers {
// Сериализует только локальную запись и проверку использования в одной поставке.
inline void CheckRouteWrite(pg::Transaction& tx, const boost::uuids::uuid& organization,
    const boost::uuids::uuid& shipment, std::optional<std::int64_t> before_registration) {
    if (!before_registration.has_value())
        return;
    tx.Execute(sql::kLockShipmentRouteUsage, organization, shipment);
    const auto current =
        tx.Execute(sql::kGetUsageCheckVersion, organization, shipment).AsSingleRow<std::int64_t>();
    if (current != before_registration.value())
        throw ApiError<409>{"route_usage_changed",
            "Маршрут поставки проверялся на изменение. Повторите сохранение"};
}
}  // namespace transportations::helpers
