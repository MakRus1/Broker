#pragma once
#include <boost/uuid/uuid.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace shipments::models::pg {
struct ShipmentSourceRecord {
    boost::uuids::uuid ShipmentId;
    userver::formats::json::Value Payload;
};
}  // namespace shipments::models::pg
