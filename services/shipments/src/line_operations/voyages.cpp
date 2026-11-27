#include <helpers/database.hpp>
#include <line_operations/voyages.hpp>

namespace shipments::line_operations {
namespace pg = userver::storages::postgres;

models::VoyageList ListVoyages(Cluster& cluster, const boost::uuids::uuid& org) {
    models::VoyageList result{};
    const auto records = cluster.Execute(pg::ClusterHostType::kMaster, sql::kListVoyages, org);
    for (const auto& row : records) {
        result.items.push_back(
            {.id = row["id"].As<boost::uuids::uuid>(), .number = row["number"].As<std::string>()});
    }
    return result;
}

models::VoyageDetail GetVoyage(
    Cluster& cluster, const boost::uuids::uuid& org, const boost::uuids::uuid& id) {
    auto tx = cluster.Begin(pg::ClusterHostType::kMaster, helpers::kReadOnlySnapshotTransaction);
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

models::ShipmentVoyageLinks ShipmentVoyages(
    Cluster& cluster, const boost::uuids::uuid& org, const boost::uuids::uuid& id) {
    models::ShipmentVoyageLinks result{};
    const auto records =
        cluster.Execute(pg::ClusterHostType::kMaster, sql::kShipmentVoyages, org, id);
    for (const auto& row : records) {
        result.items.push_back({.discharge_port = row["discharge_port"].As<std::string>(),
            .load_port = row["load_port"].As<std::string>(),
            .segment_id = row["segment_id"].As<boost::uuids::uuid>(),
            .voyage_id = row["voyage_id"].As<boost::uuids::uuid>(),
            .voyage_number = row["voyage_number"].As<std::string>()});
    }
    return result;
}
}  // namespace shipments::line_operations
