#pragma once
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace transportations {
class TransportationRegistry final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "transportations-registry";
    TransportationRegistry(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    models::TransportationListResult ListTransportations(
        const boost::uuids::uuid& organization, const models::ListTransportationsRequest&) const;
    models::RegistryValuesResult GetFilterValues(
        const boost::uuids::uuid& organization, const models::FilterValuesRequest&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace transportations

template <>
inline constexpr bool userver::components::kHasValidate<transportations::TransportationRegistry> =
    true;
