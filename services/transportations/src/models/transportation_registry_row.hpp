#pragma once
#include <models/pg/transportation_raw.hpp>
#include <string>

namespace transportations::models {
struct TransportationRegistryRow {
    models::pg::TransportationRaw Transportation;
    std::string SortKey;
};
}  // namespace transportations::models
