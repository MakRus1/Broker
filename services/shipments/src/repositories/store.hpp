#pragma once
#include <models/errors.hpp>
#include <openapi/models.hpp>
#include <platform/auth/principal.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace shipments {
// Публичные методы соответствуют конкретным сценариям; JSON не является интерфейсом хранилища.
class Store final {
  public:
    explicit Store(userver::storages::postgres::ClusterPtr cluster)
        : cluster_(std::move(cluster)) {}

    models::Shipment CreateShipment(
        const platform::auth::Principal&, const models::CreateShipmentRequest&) const;
    models::Shipment GetShipment(
        const platform::auth::Principal&, const models::GetShipmentRequest&) const;
    std::vector<models::Shipment> ListShipments(
        const platform::auth::Principal&, const models::ListShipmentsRequest&) const;
    models::Shipment UpdateShipment(
        const platform::auth::Principal&, const models::UpdateShipmentRequest&) const;
    models::ShipmentItem AddItem(
        const platform::auth::Principal&, const models::AddItemRequest&) const;
    std::vector<models::ShipmentItem> ListItems(
        const platform::auth::Principal&, const models::ListItemsRequest&) const;
    models::ShipmentItem UpdateItem(
        const platform::auth::Principal&, const models::UpdateItemRequest&) const;
    models::CreationOperation CreateOperation(
        const platform::auth::Principal&, const models::CreateOperationRequest&) const;
    models::CreationOperation GetOperation(
        const platform::auth::Principal&, const models::GetOperationRequest&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace shipments
