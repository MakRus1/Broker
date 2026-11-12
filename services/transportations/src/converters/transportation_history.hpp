#pragma once
#include <converters/transportations.hpp>
#include <models/pg/transportation_event.hpp>

namespace transportations::converters {
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
