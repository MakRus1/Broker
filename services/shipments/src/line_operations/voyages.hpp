#pragma once
#include <openapi/models.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace shipments::line_operations {
using userver::storages::postgres::Cluster;
models::VoyageList ListVoyages(Cluster& cluster, const boost::uuids::uuid& org);
models::VoyageDetail GetVoyage(
    Cluster& cluster, const boost::uuids::uuid& org, const boost::uuids::uuid& id);
models::ShipmentVoyageLinks ShipmentVoyages(
    Cluster& cluster, const boost::uuids::uuid& org, const boost::uuids::uuid& id);
}  // namespace shipments::line_operations
