#include <algorithm>
#include <components/voyages.hpp>
#include <helpers/database.hpp>

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
    result.ports = tx.Execute(sql::kVoyagePorts, org, id).AsContainer<decltype(result.ports)>();
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
