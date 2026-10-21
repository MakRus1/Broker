#include <algorithm>
#include <boost/uuid/string_generator.hpp>
#include <domain/route.hpp>
#include <repositories/rows.hpp>
#include <repositories/store.hpp>

namespace {
void ChangeActual(transportations::models::Transportation& transportation,
    const std::string& leg_id, transportations::models::EventKind event,
    const std::string& occurred_at, bool correction) {
    using namespace transportations;
    const auto requested = boost::uuids::string_generator{}(leg_id);
    const auto found = std::find_if(transportation.legs.begin(), transportation.legs.end(),
        [&](const auto& leg) { return boost::uuids::string_generator{}(leg.id) == requested; });
    if (found == transportation.legs.end())
        throw ApiError{404, "not_found", "Плечо не найдено"};
    auto& value = event == models::EventKind::Departure ? found->actual_departure_at
                                                        : found->actual_arrival_at;
    if (value.has_value() != correction)
        throw ApiError{409, "actual_conflict",
            correction ? "Исправляемый факт отсутствует"
                       : "Факт уже записан; используйте команду исправления"};
    value = occurred_at;
    domain::ValidateActualTimes(*found);
}

void SaveActual(transportations::storage::Session& session,
    transportations::models::Transportation& transportation, const std::string& leg_id) {
    using namespace transportations::storage;
    const auto requested = boost::uuids::string_generator{}(leg_id);
    const auto found = std::find_if(transportation.legs.begin(), transportation.legs.end(),
        [&](const auto& leg) { return boost::uuids::string_generator{}(leg.id) == requested; });
    session.Tx.Execute(transportations::sql::kUpdateLeg, session.Tenant, transportation.id, leg_id,
        Document(*found));
    session.Tx.Execute(
        transportations::sql::kBumpTransportationRevision, session.Tenant, transportation.id);
    ++transportation.revision;
}
}  // namespace

namespace transportations {
using namespace storage;

models::Transportation Store::SetRoute(
    const platform::auth::Principal& principal, const models::SetRouteRequest& request) const {
    Session session{cluster_, principal};
    auto result = ReadTransportation(session, request.transportation_id, true);
    CheckRevision(result.revision, request.expected_revision);
    for (const auto& leg : result.legs) {
        if (leg.actual_departure_at.has_value() || leg.actual_arrival_at.has_value())
            throw ApiError{409, "route_has_actuals", "Маршрут с фактами не заменяется"};
    }
    domain::ValidateRoute(request);
    session.Tx.Execute(sql::kDeleteLegs, session.Tenant, result.id);
    result.legs.clear();
    std::int64_t position = 0;
    for (const auto& input : request.legs) {
        auto leg = domain::MakeLeg(input);
        session.Tx.Execute(
            sql::kInsertLeg, session.Tenant, result.id, leg.id, position++, Document(leg));
        result.legs.push_back(std::move(leg));
    }
    session.Tx.Execute(sql::kBumpTransportationRevision, session.Tenant, result.id);
    ++result.revision;
    session.Audit("set_route", result.id, request, result);
    session.Tx.Commit();
    return result;
}

models::Transportation Store::RecordActual(
    const platform::auth::Principal& principal, const models::RecordActualRequest& request) const {
    Session session{cluster_, principal};
    auto result = ReadTransportation(session, request.transportation_id, true);
    CheckRevision(result.revision, request.expected_revision);
    ChangeActual(result, request.leg_id, request.event_kind, request.occurred_at, false);
    SaveActual(session, result, request.leg_id);
    session.Audit("record_actual", result.id, request, result);
    session.Tx.Commit();
    return result;
}

models::Transportation Store::CorrectActual(
    const platform::auth::Principal& principal, const models::CorrectActualRequest& request) const {
    Session session{cluster_, principal};
    auto result = ReadTransportation(session, request.transportation_id, true);
    CheckRevision(result.revision, request.expected_revision);
    ChangeActual(result, request.leg_id, request.event_kind, request.occurred_at, true);
    SaveActual(session, result, request.leg_id);
    session.Audit("correct_actual", result.id, request, result);
    session.Tx.Commit();
    return result;
}
}  // namespace transportations
