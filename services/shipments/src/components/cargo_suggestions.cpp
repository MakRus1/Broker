#include <components/cargo_suggestions.hpp>
#include <helpers/database.hpp>
#include <platform/auth/identity.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/text.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace shipments {
using namespace helpers;

CargoSuggestionsComponent::CargoSuggestionsComponent(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-shipments").GetCluster()) {
}

userver::yaml_config::Schema CargoSuggestionsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::CargoSuggestionsListResult CargoSuggestionsComponent::List(
    const models::CargoSuggestionsRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    return models::CargoSuggestionsListResult{
        .items =
            cluster_
                ->Execute(pg::ClusterHostType::kSlave, sql::kListCargoSuggestions, organization,
                    request.customer_id, userver::utils::text::ToLower(request.query.value_or("")))
                .AsContainer<std::vector<std::string>>()};
}

models::CargoSuggestionsRemoveResult CargoSuggestionsComponent::Remove(
    const models::DeleteCargoSuggestionRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kRemoveCargoSuggestion,
        organization, request.customer_id, request.value);
    return models::CargoSuggestionsRemoveResult{.deleted = !rows.IsEmpty()};
}
}  // namespace shipments
