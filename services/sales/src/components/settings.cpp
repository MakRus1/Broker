#include <components/settings.hpp>
#include <helpers/settings.hpp>
#include <models/errors.hpp>
#include <platform/document_number.hpp>
#include <sales/sql_queries.hpp>
#include <set>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/text_light.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace {
namespace models = sales::models;
using sales::ApiError;

void ValidateStages(
    const models::UpdateSalesSettingsRequest& request, const models::SalesSettings& previous) {
    std::set<std::string> ids;
    for (const auto& stage : request.stages) {
        if (!ids.insert(stage.id).second || userver::utils::text::Trim(stage.name).empty())
            throw ApiError<400>{"invalid_stage", "Этапы должны иметь уникальные коды и названия"};
    }
    // Отключение сохраняет ссылки и историю существующих карточек.
    for (const auto& stage : previous.stages)
        if (!ids.contains(stage.id))
            throw ApiError<409>{"stage_in_use", "Отключите этап вместо его удаления"};
}

}  // namespace

namespace sales {
namespace pg = userver::storages::postgres;
namespace json = userver::formats::json;

SettingsComponent::SettingsComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      cluster_(
          context.FindComponent<userver::components::Postgres>("postgres-sales").GetCluster()) {}

userver::yaml_config::Schema SettingsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::SalesSettings SettingsComponent::GetSettings(const std::string& organization) const {
    return helpers::ReadSalesSettings(
        cluster_->Execute(pg::ClusterHostType::kMaster, sql::kGetSettings, organization));
}

models::SalesSettings SettingsComponent::UpdateSettings(
    const models::UpdateSalesSettingsRequest& request) const {
    const auto& value = request;
    auto transaction = cluster_->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    transaction.Execute(sql::kLockSettings, value.organization_id);
    const auto previous =
        helpers::ReadSalesSettings(transaction.Execute(sql::kGetSettings, value.organization_id));
    if (previous.revision != value.expected_revision)
        throw ApiError<409>{"revision_conflict", "Настройки изменены. Обновите страницу"};
    ValidateStages(value, previous);
    platform::ValidateNumberTemplate(value.inquiry_number_template);
    const auto rows = transaction.Execute(sql::kSaveSettings, value.organization_id,
        value.expected_revision, json::ValueBuilder(value.stages).ExtractValue(),
        value.inquiry_number_template, value.reset_number_yearly);
    if (rows.IsEmpty())
        throw ApiError<409>{"revision_conflict", "Настройки изменены"};
    const auto result =
        helpers::ReadSalesSettings(transaction.Execute(sql::kGetSettings, value.organization_id));
    transaction.Commit();
    return result;
}

}  // namespace sales
