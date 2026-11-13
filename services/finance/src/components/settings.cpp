#include <components/settings.hpp>
#include <finance/sql_queries.hpp>
#include <helpers/exchange_amounts.hpp>
#include <helpers/finance_settings.hpp>
#include <limits>
#include <models/errors.hpp>
#include <platform/document_number.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace finance {
namespace pg = userver::storages::postgres;

SettingsComponent::SettingsComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-finance").GetCluster()) {}

userver::yaml_config::Schema SettingsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::FinanceSettings SettingsComponent::GetSettings(const std::string& organization) const {
    return helpers::LoadFinanceSettings(cluster_, organization);
}

models::FinanceSettings SettingsComponent::UpdateSettings(
    const models::UpdateFinanceSettingsRequest& request) const {
    const auto& body = request;
    helpers::ValidateCurrency(body.reporting_currency);
    platform::ValidateNumberTemplate(body.outgoing_number_template, 100);
    platform::ValidateNumberTemplate(body.incoming_number_template, 100);
    if (body.expected_revision < 0 ||
        body.expected_revision == std::numeric_limits<std::int64_t>::max())
        throw ApiError<400>{"invalid_revision", "Некорректная редакция настроек"};
    const auto rows = cluster_->Execute(pg::ClusterHostType::kMaster, sql::kSaveFinanceSettings,
        body.organization_id, body.reporting_currency, body.expected_revision,
        body.outgoing_number_template, body.incoming_number_template, body.reset_numbers_yearly);
    if (rows.IsEmpty())
        throw ApiError<409>{"revision_conflict", "Настройки изменены. Обновите форму"};
    return helpers::ReadFinanceSettings(rows);
}

}  // namespace finance
