#include <algorithm>
#include <components/voyages.hpp>
#include <helpers/database.hpp>
#include <set>

#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>

namespace shipments {
namespace pg = userver::storages::postgres;

VoyagesComponent::VoyagesComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-shipments").GetCluster()) {
}

void VoyagesComponent::ApplyAssignments(pg::Transaction& transaction,
    const boost::uuids::uuid& organization, const boost::uuids::uuid& shipment,
    const std::vector<models::VoyageAssignmentChange>& changes) {
    std::set<boost::uuids::uuid> seen;
    for (const auto& change : changes) {
        if (!seen.insert(change.segment_id).second)
            throw ApiError<400>{"duplicate_assignment", "Участок указан дважды"};
        if (!change.voyage_id.has_value() && !change.loading_call.has_value() &&
            !change.discharge_call.has_value()) {
            transaction.Execute(
                sql::kRemoveVoyageAssignment, organization, shipment, change.segment_id);
            continue;
        }
        if (!change.voyage_id.has_value() || !change.loading_call.has_value() ||
            !change.discharge_call.has_value() ||
            change.loading_call.value() >= change.discharge_call.value())
            throw ApiError<400>{
                "invalid_calls", "Выберите рейс и два последовательных по маршруту захода"};
        const auto rows = transaction.Execute(sql::kAssignVoyage, organization, shipment,
            change.segment_id, change.voyage_id.value(), change.loading_call.value(),
            change.discharge_call.value());
        if (rows.IsEmpty())
            throw ApiError<400>{
                "invalid_assignment", "Проверьте подтверждение букинга и заходы выбранного рейса"};
    }
}

models::VoyageList VoyagesComponent::ListVoyages(const boost::uuids::uuid& org) const {
    models::VoyageList result{};
    const auto records = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kListVoyages, org);
    for (const auto& row : records) {
        result.items.push_back(
            {.id = row["id"].As<boost::uuids::uuid>(), .number = row["number"].As<std::string>()});
    }
    return result;
}

models::VoyageDetail VoyagesComponent::GetVoyage(
    const boost::uuids::uuid& org, const boost::uuids::uuid& id) const {
    auto tx = cluster_->Begin(pg::ClusterHostType::kMaster, helpers::kReadOnlySnapshotTransaction);
    const auto rows = tx.Execute(sql::kGetVoyage, org, id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Рейс не найден"};
    models::VoyageDetail result{};
    result.id = id;
    result.number = rows.AsSingleRow<std::string>();
    for (const auto& row : tx.Execute(sql::kVoyagePorts, org, id))
        result.ports.push_back(
            {.name = row["name"].As<std::string>(), .position = row["position"].As<int>()});
    const auto records = tx.Execute(sql::kVoyageBookings, org, id);
    for (const auto& row : records) {
        result.bookings.push_back({.booking_number = row["booking_number"].As<std::string>(),
            .discharge_port = row["discharge_port"].As<std::string>(),
            .load_port = row["load_port"].As<std::string>(),
            .shipment_id = row["shipment_id"].As<boost::uuids::uuid>(),
            .shipment_number = row["shipment_number"].As<std::string>()});
    }
    tx.Commit();
    return result;
}

void VoyagesComponent::LoadShipmentVoyages(
    pg::Transaction& transaction, const boost::uuids::uuid& org, models::Shipment& shipment) {
    if (std::ranges::none_of(
            shipment.route_segments, [](const auto& leg) { return leg.sea.has_value(); }))
        return;
    const auto records = transaction.Execute(sql::kShipmentVoyages, org, shipment.id);
    if (records.IsEmpty())
        return;
    auto& links = shipment.confirmed_voyages.emplace();
    for (const auto& row : records) {
        links.push_back({.discharge_port = row["discharge_port"].As<std::string>(),
            .load_port = row["load_port"].As<std::string>(),
            .segment_id = row["segment_id"].As<boost::uuids::uuid>(),
            .voyage_id = row["voyage_id"].As<boost::uuids::uuid>(),
            .voyage_number = row["voyage_number"].As<std::string>()});
    }
}
}  // namespace shipments
