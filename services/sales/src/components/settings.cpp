#include <components/settings.hpp>
#include <helpers/settings.hpp>
#include <platform/auth/organization_admin.hpp>
#include <platform/document_number.hpp>
#include <platform/http_client.hpp>
#include <set>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/utils/text_light.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace sales {
using namespace helpers;

namespace {
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
    RequireAdministrator(context, request.organization_id);
    const auto result = Settings(deps.pg_cluster->Execute(
        pg::ClusterHostType::kMaster, sql::kGetSettings, request.organization_id));
    return result;
}

models::SalesSettings SettingsComponent::Update(const models::UpdateSalesSettingsRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto& value = request;
    RequireAdministrator(context, value.organization_id);
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

void SettingsComponent::RequireAdministrator(
    const userver::server::request::RequestContext& context, const std::string& organization) const
    try {
    platform::auth::RequireOrganizationAdministrator(deps_.organizations, context, organization);
} catch (const platform::http::ErrorByStatus<403>&) {

    throw ApiError<403>{"reference_forbidden", "Нет доступа к связанным данным"};
} catch (const platform::http::ErrorByStatus<404>&) {
    throw ApiError<404>{"reference_not_found", "Связанный объект не найден в организации"};
} catch (const platform::http::CancelledError&) {
    throw;
} catch (const platform::http::ApiError&) {
    throw ApiError<502>{"reference_unavailable", "Не удалось получить связанные данные"};
} catch (const platform::http::TransportError&) {
    throw ApiError<502>{"reference_unavailable", "Сервис связанных данных недоступен"};
} catch (const platform::http::ContractError&) {
    throw ApiError<502>{"reference_unavailable", "Некорректный ответ сервиса связанных данных"};
}

}  // namespace sales
