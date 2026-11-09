#include <components/internal/settings.hpp>
#include <components/settings.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace sales {
using namespace detail;

SettingsComponent::SettingsComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      deps_{.organizations = context.FindComponent<clients::organizations::Component>().GetClient(),
          .pg_cluster =
              context.FindComponent<userver::components::Postgres>("postgres-sales").GetCluster()} {
}

userver::yaml_config::Schema SettingsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::SalesSettings SettingsComponent::Get(
    const models::ListSalesRequest&, userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto result = Settings(deps.pg_cluster->Execute(pg::ClusterHostType::kMaster,
        sql::kGetSettings, platform::auth::GetOrganization(platform::auth::GetIdentity(context))));
    return result;
}

models::SalesSettings SettingsComponent::GetForOrganization(
    const models::GetSalesSettingsRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    RequireAdministrator(deps, context, request.organization_id);
    const auto result = Settings(deps.pg_cluster->Execute(
        pg::ClusterHostType::kMaster, sql::kGetSettings, request.organization_id));
    return result;
}

models::SalesSettings SettingsComponent::Update(const models::UpdateSalesSettingsRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto& value = request;
    RequireAdministrator(deps, context, value.organization_id);
    auto transaction =
        deps.pg_cluster->Begin(pg::ClusterHostType::kMaster, pg::TransactionOptions{});
    transaction.Execute(sql::kLockSettings, value.organization_id);
    const auto previous = Settings(transaction.Execute(sql::kGetSettings, value.organization_id));
    if (previous.revision != value.expected_revision)
        throw ApiError<409>{"revision_conflict", "Настройки изменены. Обновите страницу"};
    ValidateStages(value, previous);
    platform::ValidateNumberTemplate(value.inquiry_number_template);
    const auto rows = transaction.Execute(sql::kSaveSettings, value.organization_id,
        value.expected_revision, json::ValueBuilder(value.stages).ExtractValue(),
        value.inquiry_number_template, value.reset_number_yearly);
    if (rows.IsEmpty())
        throw ApiError<409>{"revision_conflict", "Настройки изменены"};
    const auto result = Settings(transaction.Execute(sql::kGetSettings, value.organization_id));
    transaction.Commit();
    return result;
}
}  // namespace sales
