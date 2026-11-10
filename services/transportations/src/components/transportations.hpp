#pragma once
#include <components/references.hpp>
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace transportations {
class TransportationsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "transportations-transportations";
    TransportationsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::TransportationHistoryResult History(models::ListHistoryRequest request,
        userver::server::request::RequestContext& context) const;
    models::TransportationResult Get(models::GetTransportationRequest request,
        userver::server::request::RequestContext& context) const;
    models::TransportationListResult List(models::ListTransportationsRequest request,
        userver::server::request::RequestContext& context) const;
    models::TransportationResult Create(models::CreateTransportationRequest request,
        userver::server::request::RequestContext& context) const;
    models::TransportationResult Update(models::UpdateTransportationRequest request,
        userver::server::request::RequestContext& context) const;
    models::ContainerFactsResult ContainerFacts(models::ContainerFactsRequest request,
        userver::server::request::RequestContext& context) const;
    models::RegistryValuesResult FilterValues(models::FilterValuesRequest request,
        userver::server::request::RequestContext& context) const;
    models::EnsureTransportationsResult EnsureCreated(models::EnsureCreatedRequest request,
        userver::server::request::RequestContext& context) const;
    models::Transportation LoadLocal(
        const userver::server::request::RequestContext&, const std::string&) const;
    models::Transportation Load(
        const userver::server::request::RequestContext&, const std::string&) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
    ReferencesComponent& references_;
};
}  // namespace transportations

template <>
inline constexpr bool userver::components::kHasValidate<transportations::TransportationsComponent> =
    true;
