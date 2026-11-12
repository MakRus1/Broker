#pragma once
#include <components/rail_route.hpp>
#include <map>
#include <models/pg/route_segment.hpp>
#include <models/pg/transportation_event.hpp>
#include <models/pg/transportation_raw.hpp>
#include <openapi/models.hpp>
#include <userver/storages/postgres/result_set.hpp>
#include <vector>

namespace transportations::converters {
inline std::optional<std::string> ConvertOwnership(
    const std::optional<models::ContainerOwnership>& value) {
    if (!value.has_value())
        return std::nullopt;
    return std::string{models::EnumValue(value.value())};
}

inline models::RouteSegment Convert(const userver::formats::json::Value& document) {
    auto result = models::ParseModelJson(document, std::type_identity<models::RouteSegment>{});
    route_rules::NormalizeRailLocations(result);
    return result;
}

inline models::Transportation Convert(const models::pg::TransportationRaw& row) {
    models::Transportation result;
    result.container_number = row.ContainerNumber;
    result.id = boost::uuids::to_string(row.Id);
    result.revision = row.Revision;
    result.shipment_id = boost::uuids::to_string(row.ShipmentId);
    if (row.ItemId.has_value())
        result.item_id = boost::uuids::to_string(row.ItemId.value());
    if (row.OperationId.has_value())
        result.operation_id = boost::uuids::to_string(row.OperationId.value());
    result.container_type_code = row.ContainerType;
    if (row.Ownership.has_value())
        result.container_ownership =
            models::ParseEnumScalar(row.Ownership.value(), models::ContainerOwnership{});
    result.type = models::ParseEnumScalar(row.Type, models::TransportationType{});
    result.hazardous = row.Hazardous;
    return result;
}

inline std::vector<models::RouteSegment> Convert(
    const std::vector<userver::formats::json::Value>& documents) {
    std::vector<models::RouteSegment> result;
    result.reserve(documents.size());
    for (const auto& document : documents)
        result.push_back(Convert(document));
    return result;
}

inline std::vector<models::Transportation> Convert(
    const std::vector<models::pg::TransportationRaw>& rows,
    const std::vector<models::pg::RouteSegment>& route_segments) {
    std::vector<models::Transportation> result;
    result.reserve(rows.size());
    std::map<boost::uuids::uuid, std::size_t> indexes;
    for (const auto& row : rows) {
        indexes.emplace(row.Id, result.size());
        result.push_back(Convert(row));
    }
    for (const auto& route_segment : route_segments)
        result.at(indexes.at(route_segment.TransportationId))
            .route_segments.push_back(Convert(route_segment.Document));
    return result;
}

inline models::TransportationEvent Convert(const models::pg::TransportationEvent& row) {
    models::TransportationEvent result;
    result.id = row.Id;
    result.actor = row.Actor;
    result.action = row.Action;
    result.occurred_at = row.OccurredAt;
    const auto& document = row.Document;
    if (document.HasMember("actor_name"))
        result.actor_name = document["actor_name"].As<std::string>();
    const auto& request = document["request"];
    if (request.HasMember("reason"))
        result.reason = request["reason"].As<std::string>();
    if (request.HasMember("route_segment_id"))
        result.route_segment_id = request["route_segment_id"].As<std::string>();
    if (request.HasMember("event_kind"))
        result.event_kind =
            models::ParseEnumScalar(request["event_kind"].As<std::string>(), models::EventKind{});
    result.revision = document["result"]["revision"].As<std::int64_t>();
    for (const auto& route_segment : document["result"]["route_segments"])
        result.route_segments.push_back(Convert(route_segment));
    return result;
}

inline std::vector<models::TransportationEvent> Convert(
    const std::vector<models::pg::TransportationEvent>& rows) {
    std::vector<models::TransportationEvent> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
        result.push_back(Convert(row));
    return result;
}
}  // namespace transportations::converters
