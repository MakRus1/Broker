#include <algorithm>
#include <boost/uuid/string_generator.hpp>
#include <domain/route.hpp>
#include <helpers/actuals.hpp>
#include <helpers/json.hpp>

namespace transportations::helpers {
std::size_t ApplyActual(models::Transportation& transportation, const std::string& route_segment_id,
    models::EventKind event, const std::string& occurred_at, ActualChange change) {
    const auto requested = boost::uuids::string_generator{}(route_segment_id);
    const auto found = std::find_if(transportation.route_segments.begin(),
        transportation.route_segments.end(), [&](const auto& route_segment) {
            return boost::uuids::string_generator{}(route_segment.id) == requested;
        });
    if (found == transportation.route_segments.end())
        throw ApiError{404, "not_found", "Плечо не найдено"};
    if (found->mode == models::TransportMode::Sea || found->mode == models::TransportMode::Air)
        throw ApiError{400, "shipment_route_owned", "Даты моря и авиа изменяются в поставке"};
    if (found->rail.has_value())
        throw ApiError{
            400, "rail_calendar_dates", "Даты ЖД-участка изменяются в его деталях без времени"};
    auto& value = event == models::EventKind::Departure ? found->actual_departure_at
                                                        : found->actual_arrival_at;
    const bool correction = change == ActualChange::Correction;
    if (value.has_value() != correction)
        throw ApiError{409, "actual_conflict",
            correction ? "Исправляемый факт отсутствует"
                       : "Факт уже записан; используйте команду исправления"};
    value = occurred_at;
    domain::ValidateActualTimes(*found);
    return static_cast<std::size_t>(std::distance(transportation.route_segments.begin(), found));
}

void UpdateActual(pg::Transaction& transaction, const std::string& organization,
    models::Transportation& transportation, const std::string& route_segment_id,
    models::EventKind event, const std::string& occurred_at, ActualChange change) {
    const auto position = ApplyActual(transportation, route_segment_id, event, occurred_at, change);
    transaction.Execute(sql::kInsertRouteSegment, organization, transportation.id, route_segment_id,
        static_cast<std::int64_t>(position), Document(transportation.route_segments.at(position)));
    transaction.Execute(sql::kBumpTransportationRevision, organization, transportation.id);
    ++transportation.revision;
}
}  // namespace transportations::helpers
