#pragma once
#include <platform/auth/principal.hpp>
#include <stdexcept>
#include <string>
#include <userver/formats/json/value.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace shipments {
using Json = userver::formats::json::Value;

struct ApiError : std::runtime_error {
    int Status;
    std::string Code;

    ApiError(int status, std::string code, const std::string& message)
        : std::runtime_error(message), Status(status), Code(std::move(code)) {}
};

class Store final {
  public:
    explicit Store(userver::storages::postgres::ClusterPtr cluster)
        : cluster_(std::move(cluster)) {}

    Json CreateShipment(const platform::auth::Principal&, const Json&) const;
    Json GetShipment(const platform::auth::Principal&, const Json&) const;
    Json ListShipments(const platform::auth::Principal&, const Json&) const;
    Json UpdateShipment(const platform::auth::Principal&, const Json&) const;
    Json AddItem(const platform::auth::Principal&, const Json&) const;
    Json ListItems(const platform::auth::Principal&, const Json&) const;
    Json UpdateItem(const platform::auth::Principal&, const Json&) const;
    Json CreateOperation(const platform::auth::Principal&, const Json&) const;
    Json GetOperation(const platform::auth::Principal&, const Json&) const;

  private:
    Json Run(const platform::auth::Principal&, const Json&,
        const userver::storages::postgres::Query&, const std::string&, bool internal,
        bool mutation) const;
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace shipments
