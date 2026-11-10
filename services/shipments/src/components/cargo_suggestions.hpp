#pragma once
#include <openapi/models.hpp>
#include <userver/components/component_base.hpp>
#include <userver/server/request/request_context.hpp>
#include <userver/storages/postgres/cluster.hpp>

namespace shipments {
class CargoSuggestionsComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "shipments-cargo-suggestions";
    CargoSuggestionsComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    models::CargoSuggestionsListResult List(const models::CargoSuggestionsRequest& request,
        userver::server::request::RequestContext& context) const;
    models::CargoSuggestionsRemoveResult Remove(const models::DeleteCargoSuggestionRequest& request,
        userver::server::request::RequestContext& context) const;

  private:
    userver::storages::postgres::ClusterPtr cluster_;
};
}  // namespace shipments

template <>
inline constexpr bool userver::components::kHasValidate<shipments::CargoSuggestionsComponent> =
    true;
