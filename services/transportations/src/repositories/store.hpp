#pragma once
#include <models/errors.hpp>
#include <openapi/models.hpp>
#include <platform/auth/principal.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace transportations {
// Публичные методы соответствуют конкретным сценариям; JSON не является интерфейсом хранилища.
class Store final {
  public:
    explicit Store(userver::storages::postgres::ClusterPtr cluster)
        : cluster_(std::move(cluster)) {}

    models::CreationOperation EnsureCreated(
        const platform::auth::Principal&, const models::EnsureCreatedRequest&) const;
    models::Transportation GetTransportation(
        const platform::auth::Principal&, const models::GetTransportationRequest&) const;
    std::vector<models::Transportation> ListTransportations(
        const platform::auth::Principal&, const models::ListTransportationsRequest&) const;
    models::Transportation SetRoute(
        const platform::auth::Principal&, const models::SetRouteRequest&) const;
    models::Transportation RecordActual(
        const platform::auth::Principal&, const models::RecordActualRequest&) const;
    models::Transportation CorrectActual(
        const platform::auth::Principal&, const models::CorrectActualRequest&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace transportations
