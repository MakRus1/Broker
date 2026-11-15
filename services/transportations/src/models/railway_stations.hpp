#pragma once
#include <map>
#include <openapi/models.hpp>
#include <string>

namespace transportations::models {
using RailwayStations = std::map<boost::uuids::uuid, RailwayStation>;
}  // namespace transportations::models
