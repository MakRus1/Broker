#pragma once
#include <platform/auth/principal.hpp>
#include <stdexcept>
#include <string>
#include <userver/formats/json/value.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace transportations {
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

    Json EnsureCreated(const platform::auth::Principal&, const Json&) const;
    Json GetTransportation(const platform::auth::Principal&, const Json&) const;
    Json ListTransportations(const platform::auth::Principal&, const Json&) const;
    Json SetRoute(const platform::auth::Principal&, const Json&) const;
    Json RecordActual(const platform::auth::Principal&, const Json&) const;
    Json CorrectActual(const platform::auth::Principal&, const Json&) const;

  private:
    Json Run(const platform::auth::Principal&, const Json&,
        const userver::storages::postgres::Query&, const std::string&, bool internal,
        bool mutation) const;
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace transportations
