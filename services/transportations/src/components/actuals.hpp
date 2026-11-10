#pragma once
#include <components/references.hpp>
#include <components/transportations.hpp>
#include <domain/actuals.hpp>
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace transportations {
class ActualsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "transportations-actuals";
    ActualsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::TransportationResult Record(models::RecordActualRequest request,
        userver::server::request::RequestContext& context) const;
    models::TransportationResult Correct(models::CorrectActualRequest request,
        userver::server::request::RequestContext& context) const;

  private:
    template <typename Request>
    models::TransportationResult Apply(
        const Request&, userver::server::request::RequestContext&, domain::ActualChange) const;
    userver::storages::postgres::ClusterPtr cluster_;
    ReferencesComponent& references_;
    TransportationsComponent& transportations_;
};
}  // namespace transportations

template <>
inline constexpr bool userver::components::kHasValidate<transportations::ActualsComponent> = true;
