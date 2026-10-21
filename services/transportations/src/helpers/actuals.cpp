#include <algorithm>
#include <boost/uuid/string_generator.hpp>
#include <domain/route.hpp>
#include <helpers/actuals.hpp>

namespace transportations::helpers {
void UpdateActual(Session& session, models::Transportation& transportation,
    const std::string& leg_id, models::EventKind event, const std::string& occurred_at,
    ActualChange change) {
    const auto requested = boost::uuids::string_generator{}(leg_id);
    const auto found = std::find_if(transportation.legs.begin(), transportation.legs.end(),
        [&](const auto& leg) { return boost::uuids::string_generator{}(leg.id) == requested; });
    if (found == transportation.legs.end())
        throw ApiError{404, "not_found", "Плечо не найдено"};
    auto& value = event == models::EventKind::Departure ? found->actual_departure_at
                                                        : found->actual_arrival_at;
    const bool correction = change == ActualChange::Correction;
    if (value.has_value() != correction)
        throw ApiError{409, "actual_conflict",
            correction ? "Исправляемый факт отсутствует"
                       : "Факт уже записан; используйте команду исправления"};
    value = occurred_at;
    domain::ValidateActualTimes(*found);
    session.Tx.Execute(
        sql::kUpdateLeg, session.Tenant, transportation.id, leg_id, Document(*found));
    session.Tx.Execute(sql::kBumpTransportationRevision, session.Tenant, transportation.id);
    ++transportation.revision;
}
}  // namespace transportations::helpers
