#include <components/settings.hpp>
#include <helpers/exchange_amounts.hpp>
#include <helpers/finance_settings.hpp>
#include <platform/auth/organization_admin.hpp>
#include <platform/document_number.hpp>
#include <platform/http_client.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace finance {
using namespace helpers;

SettingsComponent::SettingsComponent(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      deps_{.organizations = context.FindComponent<clients::organizations::Component>().GetClient(),
          .pg_cluster = context.FindComponent<userver::components::Postgres>("postgres-finance")
                            .GetCluster()} {}

userver::yaml_config::Schema SettingsComponent::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

models::FinanceSettings SettingsComponent::Get(
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto organization = platform::auth::GetOrganization(platform::auth::GetIdentity(context));
    const auto result = helpers::GetFinanceSettings(deps.pg_cluster, organization);
    return result;
}

models::FinanceSettings SettingsComponent::GetForOrganization(
    const models::GetFinanceSettingsRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    RequireAdministrator(context, request.organization_id);
    const auto result = helpers::GetFinanceSettings(deps.pg_cluster, request.organization_id);
    return result;
}

models::FinanceSettings SettingsComponent::Update(
    const models::UpdateFinanceSettingsRequest& request,
    userver::server::request::RequestContext& context) const {
    const auto& deps = deps_;
    const auto& body = request;
    RequireAdministrator(context, body.organization_id);
    helpers::ValidateCurrency(body.reporting_currency);
    platform::ValidateNumberTemplate(body.outgoing_number_template, 100);
    platform::ValidateNumberTemplate(body.incoming_number_template, 100);
    if (body.expected_revision < 0 ||
        body.expected_revision == std::numeric_limits<std::int64_t>::max())
        throw ApiError<400>{"invalid_revision", "Некорректная редакция настроек"};
    const auto rows = deps.pg_cluster->Execute(helpers::pg::ClusterHostType::kMaster,
        sql::kSaveFinanceSettings, body.organization_id, body.reporting_currency,
        body.expected_revision, body.outgoing_number_template, body.incoming_number_template,
        body.reset_numbers_yearly);
    if (rows.IsEmpty())
        throw ApiError<409>{"revision_conflict", "Настройки изменены. Обновите форму"};
    const auto result = helpers::ConvertFinanceSettings(rows);
    return result;
}

void SettingsComponent::RequireAdministrator(
    const userver::server::request::RequestContext& context,
    const std::string& organization) const {
    try {
        platform::auth::RequireOrganizationAdministrator(
            deps_.organizations, context, organization);
    } catch (const platform::http::ErrorByStatus<403>&) {
        throw ApiError<403>{"forbidden", "Нет права управления организацией"};
    } catch (const platform::http::ErrorByStatus<404>&) {
        throw ApiError<404>{"organization_not_found", "Организация не найдена"};
    } catch (const platform::http::CancelledError&) {
        throw;
    } catch (const platform::http::ApiError&) {
        throw ApiError<503>{
            "organizations_unavailable", "Не удалось проверить доступ к организации"};
    } catch (const platform::http::TransportError&) {
        throw ApiError<503>{"organizations_unavailable", "Сервис организаций недоступен"};
    } catch (const platform::http::ContractError&) {
        throw ApiError<503>{"organizations_unavailable", "Некорректный ответ сервиса организаций"};
    }
}
}  // namespace finance
