#include <components/cargo_suggestions.hpp>
#include <helpers/database.hpp>
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

std::vector<std::string> CargoSuggestionsComponent::FindSuggestions(
    const std::string& organization, const std::string& customer, const std::string& query) const {
    return cluster_
        ->Execute(pg::ClusterHostType::kSlave, sql::kListCargoSuggestions, organization, customer,
            userver::utils::text::ToLower(query))
        .AsContainer<std::vector<std::string>>();
}

bool CargoSuggestionsComponent::RemoveSuggestion(
    const std::string& organization, const std::string& customer, const std::string& value) const {
    const auto rows = cluster_->Execute(
        pg::ClusterHostType::kMaster, sql::kRemoveCargoSuggestion, organization, customer, value);
    return !rows.IsEmpty();
}
}  // namespace shipments
