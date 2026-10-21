#pragma once
#include <map>
#include <openapi/models.hpp>
#include <postgresql/types.hpp>
#include <userver/storages/postgres/result_set.hpp>
#include <vector>

namespace transportations::helpers {
inline models::Transportation Convert(const pgtypes::TransportationV2& row) {
    models::Transportation result;
    result.number = row.Number;
    result.id = boost::uuids::to_string(row.Id);
    result.revision = row.Revision;
    result.shipment_id = boost::uuids::to_string(row.ShipmentId);
    result.item_id = boost::uuids::to_string(row.ItemId);
    result.operation_id = boost::uuids::to_string(row.OperationId);
    result.container_type_code = row.ContainerType;
    result.container_ownership =
        models::ParseEnumScalar(row.Ownership, models::ContainerOwnership{});
    return result;
}

inline std::vector<models::TransportationLeg> Convert(
    const std::vector<userver::formats::json::Value>& documents) {
    std::vector<models::TransportationLeg> result;
    result.reserve(documents.size());
    for (const auto& document : documents)
        result.push_back(
            models::ParseModelJson(document, std::type_identity<models::TransportationLeg>{}));
    return result;
}

inline std::vector<models::Transportation> Convert(
    const std::vector<pgtypes::TransportationV2>& rows,
    const std::vector<pgtypes::TransportationLegV1>& legs) {
    std::vector<models::Transportation> result;
    result.reserve(rows.size());
    std::map<boost::uuids::uuid, std::size_t> indexes;
    for (const auto& row : rows) {
        indexes.emplace(row.Id, result.size());
        result.push_back(Convert(row));
    }
    for (const auto& leg : legs)
        result.at(indexes.at(leg.TransportationId))
            .legs.push_back(models::ParseModelJson(
                leg.Document, std::type_identity<models::TransportationLeg>{}));
    return result;
}

inline models::TransportationEvent Convert(const pgtypes::TransportationEventV1& row) {
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
    if (request.HasMember("leg_id"))
        result.leg_id = request["leg_id"].As<std::string>();
    if (request.HasMember("event_kind"))
        result.event_kind =
            models::ParseEnumScalar(request["event_kind"].As<std::string>(), models::EventKind{});
    result.revision = document["result"]["revision"].As<std::int64_t>();
    for (const auto& leg : document["result"]["legs"])
        result.legs.push_back(
            models::ParseModelJson(leg, std::type_identity<models::TransportationLeg>{}));
    return result;
}

inline std::vector<models::TransportationEvent> Convert(
    const std::vector<pgtypes::TransportationEventV1>& rows) {
    std::vector<models::TransportationEvent> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
        result.push_back(Convert(row));
    return result;
}
}  // namespace transportations::helpers
