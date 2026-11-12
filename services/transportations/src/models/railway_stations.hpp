#pragma once
#include <map>
#include <openapi/models.hpp>
#include <string>

namespace transportations::models {
using RailwayStations = std::map<std::string, RailwayStation>;
}  // namespace transportations::models
