#pragma once
#include <models/pg/service_charge_raw.hpp>
#include <userver/utils/datetime/date.hpp>

namespace finance::models {
struct ServiceChargeRegistryRow {
    models::pg::ServiceChargeRaw Charge;
    std::string TextKey;
    std::optional<userver::utils::datetime::Date> DateKey;
};
}  // namespace finance::models
